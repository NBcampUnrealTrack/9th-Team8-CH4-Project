#include "P48BridgeNetworkSyncComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "../../Datas/Structs/P48SwingBridgeSettings.h"

namespace P48BridgeNetworkSync
{
	constexpr float FixedSimulationDeltaTime = 1.0f / 30.0f;
	constexpr float MaxPredictionTime = 0.15f;
	constexpr float CorrectionSpeed = 18.0f;
	constexpr float PredictionDecay = 1.5f;

	FVector InterpolateControls(const TArray<FVector>& Controls, const int32 SourceIndex, const int32 SourceCount)
	{
		if (Controls.IsEmpty() || SourceCount < 2)
		{
			return FVector::ZeroVector;
		}
		if (Controls.Num() == 1)
		{
			return Controls[0];
		}
		const float ControlPosition = static_cast<float>(SourceIndex) * static_cast<float>(Controls.Num() - 1) / static_cast<float>(SourceCount - 1);
		const int32 Lower = FMath::Clamp(FMath::FloorToInt(ControlPosition), 0, Controls.Num() - 1);
		const int32 Upper = FMath::Min(Lower + 1, Controls.Num() - 1);
		return FMath::Lerp(Controls[Lower], Controls[Upper], ControlPosition - Lower);
	}
}

UP48BridgeNetworkSyncComponent::UP48BridgeNetworkSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UP48BridgeNetworkSyncComponent::ResetInterpolation()
{
	TargetLeftOffsets.Reset();
	TargetRightOffsets.Reset();
	CurrentLeftOffsets.Reset();
	CurrentRightOffsets.Reset();
	TargetLeftVelocities.Reset();
	TargetRightVelocities.Reset();

	LastServerTimeSeconds = 0.0f;
	LocalReceiveTimeSeconds = 0.0f;

	ActiveGenerationId = INDEX_NONE;
	LastSimulationFrame = 0;
	SourceNodeCount = 0;
	bHasNetworkState = false;
}

void UP48BridgeNetworkSyncComponent::BuildNetworkState(const TArray<FP48BridgePlankNode>& Nodes, int32 GenerationId, uint16 SimulationFrame, const int32 MaxControlPoints, FP48BridgeNetworkState& OutState) const
{
	OutState.GenerationId = GenerationId;
	OutState.SimulationFrame = SimulationFrame;
	OutState.ServerTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	OutState.SourceNodeCount = Nodes.Num();

	const int32 ControlCount = FMath::Clamp(MaxControlPoints, 2, Nodes.Num());
	OutState.ControlPoints.Reset();
	OutState.ControlPoints.Reserve(ControlCount);

	for (int32 ControlIndex = 0; ControlIndex < ControlCount; ++ControlIndex)
	{
		const int32 SourceIndex = FMath::RoundToInt(static_cast<float>(ControlIndex) * static_cast<float>(Nodes.Num() - 1) / static_cast<float>(ControlCount - 1));
		const FP48BridgePlankNode& Node = Nodes[SourceIndex];
		FP48BridgeNetworkNodeState& NetworkNode = OutState.ControlPoints.Emplace_GetRef();

		NetworkNode.LeftOffset = Node.CurrentLeft - Node.RestLeft;
		NetworkNode.RightOffset = Node.CurrentRight - Node.RestRight;

		NetworkNode.LeftVelocity =
			(Node.CurrentLeft - Node.PreviousLeft) /
			P48BridgeNetworkSync::FixedSimulationDeltaTime;

		NetworkNode.RightVelocity =
			(Node.CurrentRight - Node.PreviousRight) /
			P48BridgeNetworkSync::FixedSimulationDeltaTime;
	}
}

void UP48BridgeNetworkSyncComponent::ReceiveNetworkState(const FP48BridgeNetworkState& State)
{
	if (State.SourceNodeCount < 2 || State.ControlPoints.Num() < 2 || State.ControlPoints.Num() > State.SourceNodeCount)
	{
		return;
	}

	if (bHasNetworkState &&
		State.GenerationId == ActiveGenerationId &&
		State.SimulationFrame == LastSimulationFrame)
	{
		return;
	}

	const bool bReset =
		!bHasNetworkState ||
		State.GenerationId != ActiveGenerationId ||
		State.SourceNodeCount != SourceNodeCount;

	TargetLeftOffsets.SetNum(State.ControlPoints.Num());
	TargetRightOffsets.SetNum(State.ControlPoints.Num());
	TargetLeftVelocities.SetNum(State.ControlPoints.Num());
	TargetRightVelocities.SetNum(State.ControlPoints.Num());

	for (int32 Index = 0; Index < State.ControlPoints.Num(); ++Index)
	{
		TargetLeftOffsets[Index] = State.ControlPoints[Index].LeftOffset;
		TargetRightOffsets[Index] = State.ControlPoints[Index].RightOffset;

		TargetLeftVelocities[Index] = State.ControlPoints[Index].LeftVelocity;
		TargetRightVelocities[Index] = State.ControlPoints[Index].RightVelocity;
	}

	if (bReset)
	{
		CurrentLeftOffsets.SetNum(State.SourceNodeCount);
		CurrentRightOffsets.SetNum(State.SourceNodeCount);
		for (int32 SourceIndex = 0; SourceIndex < State.SourceNodeCount; ++SourceIndex)
		{
			CurrentLeftOffsets[SourceIndex] = P48BridgeNetworkSync::InterpolateControls(TargetLeftOffsets, SourceIndex, State.SourceNodeCount);
			CurrentRightOffsets[SourceIndex] = P48BridgeNetworkSync::InterpolateControls(TargetRightOffsets, SourceIndex, State.SourceNodeCount);
		}
	}

	LastServerTimeSeconds = State.ServerTimeSeconds;

	if (const UWorld* World = GetWorld())
	{
		LocalReceiveTimeSeconds = World->GetTimeSeconds();
	}

	ActiveGenerationId = State.GenerationId;
	LastSimulationFrame = State.SimulationFrame;
	SourceNodeCount = State.SourceNodeCount;
	bHasNetworkState = true;
}

bool UP48BridgeNetworkSyncComponent::CalculateClientNodes(const float DeltaSeconds, const TArray<FP48BridgePlankNode>& RestNodes, TArray<FP48BridgePlankNode>& OutNodes)
{
    if (!bHasNetworkState ||
        RestNodes.Num() != SourceNodeCount ||
        TargetLeftOffsets.Num() < 2 ||
		CurrentLeftOffsets.Num() != SourceNodeCount)
    {
        return false;
    }

    const float StateAge = GetServerStateAge();

    const float PredictionTime =
        FMath::Clamp(
            StateAge,
            0.0f,
            P48BridgeNetworkSync::MaxPredictionTime);

    float PoseDecay = 1.0f;

    if (StateAge > P48BridgeNetworkSync::MaxPredictionTime)
    {
        const float ExcessTime =
            StateAge - P48BridgeNetworkSync::MaxPredictionTime;

        PoseDecay =
            FMath::Exp(
                -P48BridgeNetworkSync::PredictionDecay *
                ExcessTime);
    }

    const float CorrectionAlpha =
        1.0f -
        FMath::Exp(
            -P48BridgeNetworkSync::CorrectionSpeed *
            FMath::Max(0.0f, DeltaSeconds));

    for (int32 Index = 0; Index < SourceNodeCount; ++Index)
    {
		const FVector TargetLeft = P48BridgeNetworkSync::InterpolateControls(TargetLeftOffsets, Index, SourceNodeCount);
		const FVector TargetRight = P48BridgeNetworkSync::InterpolateControls(TargetRightOffsets, Index, SourceNodeCount);
		const FVector LeftVelocity = P48BridgeNetworkSync::InterpolateControls(TargetLeftVelocities, Index, SourceNodeCount);
		const FVector RightVelocity = P48BridgeNetworkSync::InterpolateControls(TargetRightVelocities, Index, SourceNodeCount);
		const FVector DesiredLeft = (TargetLeft + LeftVelocity * PredictionTime) * PoseDecay;

		const FVector DesiredRight = (TargetRight + RightVelocity * PredictionTime) * PoseDecay;

        CurrentLeftOffsets[Index] =
            FMath::Lerp(
                CurrentLeftOffsets[Index],
                DesiredLeft,
                CorrectionAlpha);

        CurrentRightOffsets[Index] =
            FMath::Lerp(
                CurrentRightOffsets[Index],
                DesiredRight,
                CorrectionAlpha);
    }

    OutNodes = RestNodes;

    for (int32 Index = 0; Index < OutNodes.Num(); ++Index)
    {
        FP48BridgePlankNode& Node = OutNodes[Index];

        Node.CurrentLeft =
            Node.RestLeft +
            CurrentLeftOffsets[Index];

        Node.CurrentRight =
            Node.RestRight +
            CurrentRightOffsets[Index];

        Node.PreviousLeft = Node.CurrentLeft;
        Node.PreviousRight = Node.CurrentRight;

        Node.AccumulatedLeftAcceleration = FVector::ZeroVector;
        Node.AccumulatedRightAcceleration = FVector::ZeroVector;
    }

    return true;
}

float UP48BridgeNetworkSyncComponent::GetServerStateAge() const
{
	const UWorld* World = GetWorld();
	
	if (!World) return 0.0f;
	
	if (const AGameStateBase* GameState = World->GetGameState()) return FMath::Max(0.0f, GameState->GetServerWorldTimeSeconds() - LastServerTimeSeconds);
	
	return FMath::Max(0.f, World->GetTimeSeconds() - LocalReceiveTimeSeconds);
}
