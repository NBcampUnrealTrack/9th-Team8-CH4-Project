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
