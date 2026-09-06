#include "P48BridgePhysicsComponent.h"

#include "../Layout/P48BridgeLayoutComponent.h"

struct FP48BridgeSolverValues
{
	float Damping = 0.95f;
	float RestoringAcceleration = 10.0f;
	float GravityScale = 0.24f;
	float MaxDisplacement = 200.0f;
	int32 Iterations = 6;
};

static FP48BridgeSolverValues GetSolverValues(const EP48BridgeBehavior Behavior)
{
	switch (Behavior)
	{
	case EP48BridgeBehavior::Tight: return { 0.90f, 18.0f, 0.12f, 120.0f, 8 };
	case EP48BridgeBehavior::Loose: return { 0.975f, 5.0f, 0.38f, 300.0f, 4 };
	default: return { 0.95f, 10.0f, 0.24f, 200.0f, 6 };
	}
}

UP48BridgePhysicsComponent::UP48BridgePhysicsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UP48BridgePhysicsComponent::Initialize(const FP48BridgeLayoutResult& LayoutResult)
{
	Nodes = LayoutResult.Nodes;
	TimeAccumulator = 0.0f;
}

void UP48BridgePhysicsComponent::ResetSimulation()
{
	Nodes.Reset();
	TimeAccumulator = 0.0f;
}

bool UP48BridgePhysicsComponent::Advance(const float DeltaSeconds, const EP48BridgeBehavior Behavior)
{
	if (!HasNodes() || DeltaSeconds <= 0.0f)
	{
		return false;
	}
	constexpr float FixedDeltaTime = 1.0f / 30.0f;
	TimeAccumulator = FMath::Min(TimeAccumulator + DeltaSeconds, FixedDeltaTime * 4.0f);
	bool bAdvanced = false;
	while (TimeAccumulator >= FixedDeltaTime)
	{
		SimulateStep(FixedDeltaTime, Behavior);
		TimeAccumulator -= FixedDeltaTime;
		bAdvanced = true;
	}
	return bAdvanced;
}

void UP48BridgePhysicsComponent::AddImpulseAtLocation(const FVector& WorldLocation, const FVector& WorldImpulse)
{
	if (!HasNodes())
	{
		return;
	}
	int32 NearestIndex = INDEX_NONE;
	float NearestDistanceSquared = MAX_flt;
	for (int32 Index = 1; Index + 1 < Nodes.Num(); ++Index)
	{
		const FVector Center = (Nodes[Index].CurrentLeft + Nodes[Index].CurrentRight) * 0.5f;
		const float DistanceSquared = FVector::DistSquared(Center, WorldLocation);
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestIndex = Index;
		}
	}
	if (!Nodes.IsValidIndex(NearestIndex))
	{
		return;
	}

	FP48BridgePlankNode& Node = Nodes[NearestIndex];
	const FVector Width = Node.CurrentRight - Node.CurrentLeft;
	const float WidthSquared = Width.SizeSquared();
	const float RightWeight = WidthSquared > UE_KINDA_SMALL_NUMBER ? FMath::Clamp(FVector::DotProduct(WorldLocation - Node.CurrentLeft, Width) / WidthSquared, 0.0f, 1.0f) : 0.5f;
	Node.AccumulatedLeftAcceleration += WorldImpulse * (1.0f - RightWeight);
	Node.AccumulatedRightAcceleration += WorldImpulse * RightWeight;
}

void UP48BridgePhysicsComponent::AddImpactAtLocation(const FVector& WorldLocation, const FVector& NormalImpulse, const FVector& OtherVelocity, const float ImpactStrength)
{
	const FVector Impact = NormalImpulse.IsNearlyZero() ? OtherVelocity * 40.0f - FVector::UpVector * 12000.0f : NormalImpulse;
	AddImpulseAtLocation(WorldLocation, Impact * ImpactStrength);
}

bool UP48BridgePhysicsComponent::IsSettled(float MotionThreshold) const
{
	if (!HasNodes())
	{
		return true;
	}

	const float ThresholdSquared = FMath::Square(MotionThreshold);

	for (const FP48BridgePlankNode& Node : Nodes)
	{
		if (Node.bFixed)
		{
			continue;
		}

		const float LeftMotionSquared = FVector::DistSquared(Node.CurrentLeft, Node.PreviousLeft);
		const float RightMotionSquared = FVector::DistSquared(Node.CurrentRight, Node.PreviousRight);

		if (LeftMotionSquared > ThresholdSquared || RightMotionSquared > ThresholdSquared)
		{
			return false;
		}

		if (!Node.AccumulatedLeftAcceleration.IsNearlyZero() || !Node.AccumulatedRightAcceleration.IsNearlyZero())
		{
			return false;
		}
	}

	return true;
}

void UP48BridgePhysicsComponent::SimulateStep(const float FixedDeltaTime, const EP48BridgeBehavior Behavior)
{
	const FP48BridgeSolverValues Values = GetSolverValues(Behavior);
	const float DeltaTimeSquared = FixedDeltaTime * FixedDeltaTime;
	for (FP48BridgePlankNode& Node : Nodes)
	{
		if (Node.bFixed)
		{
			Node.CurrentLeft = Node.PreviousLeft = Node.RestLeft;
			Node.CurrentRight = Node.PreviousRight = Node.RestRight;
			Node.AccumulatedLeftAcceleration = Node.AccumulatedRightAcceleration = FVector::ZeroVector;
			continue;
		}
		const FVector LeftVelocity = (Node.CurrentLeft - Node.PreviousLeft) * Values.Damping;
		const FVector RightVelocity = (Node.CurrentRight - Node.PreviousRight) * Values.Damping;
		const FVector LeftAcceleration = FVector(0.0f, 0.0f, -980.0f * Values.GravityScale) + (Node.RestLeft - Node.CurrentLeft) * Values.RestoringAcceleration + Node.AccumulatedLeftAcceleration;
		const FVector RightAcceleration = FVector(0.0f, 0.0f, -980.0f * Values.GravityScale) + (Node.RestRight - Node.CurrentRight) * Values.RestoringAcceleration + Node.AccumulatedRightAcceleration;
		Node.PreviousLeft = Node.CurrentLeft;
		Node.PreviousRight = Node.CurrentRight;
		Node.CurrentLeft += LeftVelocity + LeftAcceleration * DeltaTimeSquared;
		Node.CurrentRight += RightVelocity + RightAcceleration * DeltaTimeSquared;
		Node.AccumulatedLeftAcceleration = Node.AccumulatedRightAcceleration = FVector::ZeroVector;
	}

	for (int32 Iteration = 0; Iteration < Values.Iterations; ++Iteration)
	{
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			FP48BridgePlankNode& Node = Nodes[Index];
			const float InverseMass = Node.bFixed ? 0.0f : 1.0f;
			SolveDistance(Node.CurrentLeft, Node.CurrentRight, FVector::Distance(Node.RestLeft, Node.RestRight), InverseMass, InverseMass);
			if (Index + 1 < Nodes.Num())
			{
				FP48BridgePlankNode& Next = Nodes[Index + 1];
				const float NextInverseMass = Next.bFixed ? 0.0f : 1.0f;
				SolveDistance(Node.CurrentLeft, Next.CurrentLeft, FVector::Distance(Node.RestLeft, Next.RestLeft), InverseMass, NextInverseMass);
				SolveDistance(Node.CurrentRight, Next.CurrentRight, FVector::Distance(Node.RestRight, Next.RestRight), InverseMass, NextInverseMass);
			}
		}
		for (FP48BridgePlankNode& Node : Nodes)
		{
			if (Node.bFixed)
			{
				Node.CurrentLeft = Node.RestLeft;
				Node.CurrentRight = Node.RestRight;
			}
			else
			{
				Node.CurrentLeft = Node.RestLeft + (Node.CurrentLeft - Node.RestLeft).GetClampedToMaxSize(Values.MaxDisplacement);
				Node.CurrentRight = Node.RestRight + (Node.CurrentRight - Node.RestRight).GetClampedToMaxSize(Values.MaxDisplacement);
			}
		}
	}
}

void UP48BridgePhysicsComponent::SolveDistance(FVector& A, FVector& B, const float RestDistance, const float InverseMassA, const float InverseMassB) const
{
	const FVector Delta = B - A;
	const float Distance = Delta.Length();
	const float TotalInverseMass = InverseMassA + InverseMassB;
	if (Distance <= UE_KINDA_SMALL_NUMBER || TotalInverseMass <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Correction = Delta * ((Distance - RestDistance) / Distance);
	A += Correction * (InverseMassA / TotalInverseMass);
	B -= Correction * (InverseMassB / TotalInverseMass);
}
