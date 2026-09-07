#include "P48BridgeNetworkSyncComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "../../Datas/Structs/P48SwingBridgeSettings.h"

namespace P48BridgeNetworkSync
{
	constexpr float FixedSimulationDeltaTime = 1.0f / 30.0f;
	constexpr float MaxPredictionTime = 0.10f;
	constexpr float CorrectionSpeed = 18.0f;
	constexpr float PredictionDecay = 4.0f;
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

void UP48BridgeNetworkSyncComponent::BuildNetworkState(const TArray<FP48BridgePlankNode>& Nodes, int32 GenerationId, uint16 SimulationFrame, FP48BridgeNetworkState& OutState) const
{
	OutState.GenerationId = GenerationId;
	OutState.SimulationFrame = SimulationFrame;
	OutState.ServerTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	OutState.SourceNodeCount = Nodes.Num();

	OutState.Nodes.Reset();
	OutState.Nodes.Reserve(Nodes.Num());

	for (const FP48BridgePlankNode& Node : Nodes)
	{
		FP48BridgeNetworkNodeState& NetworkNode = OutState.Nodes.Emplace_GetRef();

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
	if (State.SourceNodeCount < 2 || State.Nodes.Num() != State.SourceNodeCount)
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

	TargetLeftOffsets.SetNum(State.Nodes.Num());
	TargetRightOffsets.SetNum(State.Nodes.Num());
	TargetLeftVelocities.SetNum(State.Nodes.Num());
	TargetRightVelocities.SetNum(State.Nodes.Num());

	for (int32 Index = 0; Index < State.Nodes.Num(); ++Index)
	{
		TargetLeftOffsets[Index] = State.Nodes[Index].LeftOffset;
		TargetRightOffsets[Index] = State.Nodes[Index].RightOffset;

		TargetLeftVelocities[Index] = State.Nodes[Index].LeftVelocity;
		TargetRightVelocities[Index] = State.Nodes[Index].RightVelocity;
	}

	if (bReset)
	{
		CurrentLeftOffsets = TargetLeftOffsets;
		CurrentRightOffsets = TargetRightOffsets;
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
        TargetLeftOffsets.Num() != SourceNodeCount)
    {
        return false;
    }

    const float StateAge = GetServerStateAge();

    const float PredictionTime =
        FMath::Clamp(
            StateAge,
            0.0f,
            P48BridgeNetworkSync::MaxPredictionTime);

    float VelocityScale = 1.0f;

    if (StateAge > P48BridgeNetworkSync::MaxPredictionTime)
    {
        const float ExcessTime =
            StateAge - P48BridgeNetworkSync::MaxPredictionTime;

        VelocityScale =
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
        const FVector DesiredLeft =
            TargetLeftOffsets[Index] +
            TargetLeftVelocities[Index] *
            PredictionTime *
            VelocityScale;

        const FVector DesiredRight =
            TargetRightOffsets[Index] +
            TargetRightVelocities[Index] *
            PredictionTime *
            VelocityScale;

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
