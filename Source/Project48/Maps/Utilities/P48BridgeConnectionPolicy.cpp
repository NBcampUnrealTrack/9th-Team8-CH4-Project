#include "P48BridgeConnectionPolicy.h"

#include "../Datas/Structs/P48BridgeConnectionRules.h"

bool P48BridgeConnectionPolicy::IsGeometryValid(
	const FVector& Start,
	const FVector& End,
	const FP48BridgeConnectionRules& Rules,
	float& OutLength)
{
	const FVector Difference = End - Start;
	const float HorizontalDistance = FVector2D(Difference.X, Difference.Y).Length();
	const float HeightDifference = FMath::Abs(Difference.Z);
	OutLength = Difference.Length();

	if (HorizontalDistance < Rules.MinHorizontalDistance ||
		HeightDifference > Rules.MaxHeightDifference ||
		OutLength > Rules.MaxBridgeLength)
	{
		return false;
	}

	const float SlopeAngle = FMath::RadiansToDegrees(
		FMath::Atan2(HeightDifference, HorizontalDistance));
	return SlopeAngle <= Rules.MaxSlopeAngle;
}

bool P48BridgeConnectionPolicy::AreSameUndirectedEndpoints(
	const FVector& StartA,
	const FVector& EndA,
	const FVector& StartB,
	const FVector& EndB,
	const float Tolerance)
{
	const float SafeTolerance = FMath::Max(0.0f, Tolerance);
	const bool bSameDirection =
		StartA.Equals(StartB, SafeTolerance) && EndA.Equals(EndB, SafeTolerance);
	const bool bReverseDirection =
		StartA.Equals(EndB, SafeTolerance) && EndA.Equals(StartB, SafeTolerance);
	return bSameDirection || bReverseDirection;
}

void P48BridgeConnectionPolicy::SortEndpointCandidates(TArray<FP48BridgeEndpointCandidate>& Candidates)
{
	Candidates.Sort([](const FP48BridgeEndpointCandidate& Left, const FP48BridgeEndpointCandidate& Right)
	{
		const float LeftCommonHeight = FMath::Min(Left.Start.Z, Left.End.Z);
		const float RightCommonHeight = FMath::Min(Right.Start.Z, Right.End.Z);
		if (LeftCommonHeight != RightCommonHeight) { return LeftCommonHeight > RightCommonHeight; }

		const float LeftHeightDifference = FMath::Abs(Left.Start.Z - Left.End.Z);
		const float RightHeightDifference = FMath::Abs(Right.Start.Z - Right.End.Z);
		if (LeftHeightDifference != RightHeightDifference) { return LeftHeightDifference < RightHeightDifference; }
		if (Left.Length != Right.Length) { return Left.Length < Right.Length; }

		if (Left.Start.X != Right.Start.X) { return Left.Start.X < Right.Start.X; }
		if (Left.Start.Y != Right.Start.Y) { return Left.Start.Y < Right.Start.Y; }
		if (Left.Start.Z != Right.Start.Z) { return Left.Start.Z > Right.Start.Z; }
		if (Left.End.X != Right.End.X) { return Left.End.X < Right.End.X; }
		if (Left.End.Y != Right.End.Y) { return Left.End.Y < Right.End.Y; }
		return Left.End.Z > Right.End.Z;
	});
}

bool P48BridgeConnectionPolicy::IsCandidateAvailable(
	const FP48BridgeEndpointCandidate& Candidate,
	const int32 StartIslandIndex,
	const int32 EndIslandIndex,
	const TMap<int32, TArray<FVector>>& UsedEndpointsByIsland,
	const float EndpointExclusionRadius)
{
	const float SafeRadius = FMath::Max(0.0f, EndpointExclusionRadius);
	auto IsEndpointAvailable = [&](const int32 IslandIndex, const FVector& Location)
	{
		if (SafeRadius <= UE_KINDA_SMALL_NUMBER)
		{
			return true;
		}
		const TArray<FVector>* UsedEndpoints = UsedEndpointsByIsland.Find(IslandIndex);
		if (!UsedEndpoints)
		{
			return true;
		}
		return !UsedEndpoints->ContainsByPredicate([&](const FVector& UsedLocation)
		{
			return FVector2D::DistSquared(FVector2D(Location), FVector2D(UsedLocation)) <= FMath::Square(SafeRadius);
		});
	};

	return IsEndpointAvailable(StartIslandIndex, Candidate.Start) &&
		IsEndpointAvailable(EndIslandIndex, Candidate.End);
}

int32 P48BridgeConnectionPolicy::FindFirstAvailableCandidate(
	const TArray<FP48BridgeEndpointCandidate>& Candidates,
	const int32 StartIslandIndex,
	const int32 EndIslandIndex,
	const TMap<int32, TArray<FVector>>& UsedEndpointsByIsland,
	const float EndpointExclusionRadius)
{
	for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
	{
		if (IsCandidateAvailable(
			Candidates[CandidateIndex],
			StartIslandIndex,
			EndIslandIndex,
			UsedEndpointsByIsland,
			EndpointExclusionRadius))
		{
			return CandidateIndex;
		}
	}
	return INDEX_NONE;
}

void P48BridgeConnectionPolicy::ReserveCandidateEndpoints(
	const FP48BridgeEndpointCandidate& Candidate,
	const int32 StartIslandIndex,
	const int32 EndIslandIndex,
	TMap<int32, TArray<FVector>>& UsedEndpointsByIsland)
{
	UsedEndpointsByIsland.FindOrAdd(StartIslandIndex).Add(Candidate.Start);
	UsedEndpointsByIsland.FindOrAdd(EndIslandIndex).Add(Candidate.End);
}

bool P48BridgeConnectionPolicy::TryCalculateAlignedIslandHeight(
	const float ParentBaseHeight,
	const float ParentAnchorOffset,
	const float ChildAnchorOffset,
	const float MinBaseHeight,
	const float MaxBaseHeight,
	float& OutChildBaseHeight)
{
	OutChildBaseHeight = ParentBaseHeight + ParentAnchorOffset - ChildAnchorOffset;
	return OutChildBaseHeight >= MinBaseHeight && OutChildBaseHeight <= MaxBaseHeight;
}
