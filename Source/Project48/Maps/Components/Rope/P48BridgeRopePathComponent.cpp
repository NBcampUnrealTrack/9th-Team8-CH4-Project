#include "P48BridgeRopePathComponent.h"

#include "../../Datas/Structs/P48SwingBridgeSettings.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"

UP48BridgeRopePathComponent::UP48BridgeRopePathComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UP48BridgeRopePathComponent::ResetPostSockets()
{
	UpperSockets.Reset();
	LowerSockets.Reset();
	bGenerateLowerRopes = false;
}

bool UP48BridgeRopePathComponent::ConfigurePostSockets(const TArray<TObjectPtr<UStaticMeshComponent>>& Posts, const FP48BridgePostConnectionSettings& Settings, FString& OutError)
{
	ResetPostSockets();
	OutError.Reset();
	if (!Settings.bUsePostSockets) { return true; }
	if (Posts.Num() != 4)
	{
		OutError = TEXT("Assign Settings/Assets/Anchor Post/Mesh (SM_Bridge_AnchorPost). Four posts are required.");
		return false;
	}
	for (const UStaticMeshComponent* Post : Posts)
	{
		if (!IsValid(Post) || !Post->DoesSocketExist(Settings.UpperSocketName) || !Post->DoesSocketExist(Settings.LowerSocketName))
		{
			OutError = FString::Printf(TEXT("Post socket missing: upper=%s, lower=%s (lower enabled=%d). Check the assigned Anchor Post mesh."), *Settings.UpperSocketName.ToString(), *Settings.LowerSocketName.ToString(), Settings.bGenerateLowerRopes);
			ResetPostSockets();
			return false;
		}
		UpperSockets.Add(Post->GetSocketTransform(Settings.UpperSocketName, RTS_World));
		LowerSockets.Add(Post->GetSocketTransform(Settings.LowerSocketName, RTS_World));
	}
	bGenerateLowerRopes = Settings.bGenerateLowerRopes;
	return true;
}

bool UP48BridgeRopePathComponent::CalculatePaths(const TArray<FP48BridgePlankNode>& Nodes, const float MainRopeHeight, const float CollisionRadius, FP48BridgeRopePathResult& OutResult) const
{
	OutResult.Reset();
	const AActor* Owner = GetOwner();
	if (!Owner || Nodes.Num() < 2)
	{
		return false;
	}
	const FTransform OwnerTransform = Owner->GetActorTransform();
	OutResult.LeftMainPoints.Reserve(Nodes.Num());
	OutResult.RightMainPoints.Reserve(Nodes.Num());
	OutResult.ConnectorSegments.Reserve(Nodes.Num() * 2);
	TArray<FVector> LeftLowerPoints;
	TArray<FVector> RightLowerPoints;
	if (UpperSockets.Num() == 4)
	{
		OutResult.LeftMainPoints.Add(OwnerTransform.InverseTransformPosition(UpperSockets[0].GetLocation()));
		OutResult.RightMainPoints.Add(OwnerTransform.InverseTransformPosition(UpperSockets[1].GetLocation()));
	}
	if (bGenerateLowerRopes)
	{
		LeftLowerPoints.Add(OwnerTransform.InverseTransformPosition(LowerSockets[0].GetLocation()));
		RightLowerPoints.Add(OwnerTransform.InverseTransformPosition(LowerSockets[1].GetLocation()));
	}
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const FP48BridgePlankNode& Node = Nodes[Index];
		const float Alpha = static_cast<float>(Index) / (Nodes.Num() - 1);
		FVector LeftRopeWorld = Node.CurrentLeft + FVector::UpVector * MainRopeHeight;
		FVector RightRopeWorld = Node.CurrentRight + FVector::UpVector * MainRopeHeight;
		if (UpperSockets.Num() == 4 && LowerSockets.Num() == 4)
		{
			// 각 판자 위에 세로 연결점을 두고 소켓은 독립적인 경로 끝점으로 둡니다.
			const float LeftHeight = FMath::Lerp(UpperSockets[0].GetLocation().Z - LowerSockets[0].GetLocation().Z, UpperSockets[2].GetLocation().Z - LowerSockets[2].GetLocation().Z, Alpha);
			const float RightHeight = FMath::Lerp(UpperSockets[1].GetLocation().Z - LowerSockets[1].GetLocation().Z, UpperSockets[3].GetLocation().Z - LowerSockets[3].GetLocation().Z, Alpha);
			LeftRopeWorld = Node.CurrentLeft + FVector::UpVector * LeftHeight;
			RightRopeWorld = Node.CurrentRight + FVector::UpVector * RightHeight;
		}
		if (bGenerateLowerRopes)
		{
			LeftLowerPoints.Add(OwnerTransform.InverseTransformPosition(Node.CurrentLeft));
			RightLowerPoints.Add(OwnerTransform.InverseTransformPosition(Node.CurrentRight));
		}
		OutResult.LeftMainPoints.Add(OwnerTransform.InverseTransformPosition(LeftRopeWorld));
		OutResult.RightMainPoints.Add(OwnerTransform.InverseTransformPosition(RightRopeWorld));
		if (UpperSockets.Num() == 4 || MainRopeHeight > UE_KINDA_SMALL_NUMBER)
		{
			OutResult.ConnectorSegments.Add(BuildStraightSegment(Node.CurrentLeft, LeftRopeWorld, CollisionRadius));
			OutResult.ConnectorSegments.Add(BuildStraightSegment(Node.CurrentRight, RightRopeWorld, CollisionRadius));
		}
	}
	if (UpperSockets.Num() == 4)
	{
		OutResult.LeftMainPoints.Add(OwnerTransform.InverseTransformPosition(UpperSockets[2].GetLocation()));
		OutResult.RightMainPoints.Add(OwnerTransform.InverseTransformPosition(UpperSockets[3].GetLocation()));
	}
	if (bGenerateLowerRopes)
	{
		LeftLowerPoints.Add(OwnerTransform.InverseTransformPosition(LowerSockets[2].GetLocation()));
		RightLowerPoints.Add(OwnerTransform.InverseTransformPosition(LowerSockets[3].GetLocation()));
	}
	BuildMainSegments(OutResult.LeftMainPoints, CollisionRadius, OutResult.LeftMainSegments);
	BuildMainSegments(OutResult.RightMainPoints, CollisionRadius, OutResult.RightMainSegments);
	BuildMainSegments(LeftLowerPoints, CollisionRadius, OutResult.LeftLowerSegments);
	BuildMainSegments(RightLowerPoints, CollisionRadius, OutResult.RightLowerSegments);
	return !OutResult.LeftMainSegments.IsEmpty() && !OutResult.RightMainSegments.IsEmpty();
}

void UP48BridgeRopePathComponent::BuildMainSegments(const TArray<FVector>& Points, const float CollisionRadius, TArray<FP48BridgeRopeSegment>& OutSegments) const
{
	OutSegments.Reset(FMath::Max(0, Points.Num() - 1));
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	const FTransform OwnerTransform = Owner->GetActorTransform();
	for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
	{
		FP48BridgeRopeSegment& Segment = OutSegments.Emplace_GetRef();
		Segment.Start = Points[Index];
		Segment.End = Points[Index + 1];
		const FVector Previous = Points[FMath::Max(0, Index - 1)];
		const FVector Next = Points[FMath::Min(Points.Num() - 1, Index + 2)];
		Segment.StartTangent = (Points[Index + 1] - Previous) * 0.5f;
		Segment.EndTangent = (Next - Points[Index]) * 0.5f;
		const FVector Chord = Segment.End - Segment.Start;
		Segment.StartTangent = Index == 0 ? Chord : Segment.StartTangent.GetClampedToMaxSize(Chord.Length());
		Segment.EndTangent = Index + 2 == Points.Num() ? Chord : Segment.EndTangent.GetClampedToMaxSize(Chord.Length());
		const FVector WorldStart = OwnerTransform.TransformPosition(Segment.Start);
		const FVector WorldEnd = OwnerTransform.TransformPosition(Segment.End);
		const FVector WorldDelta = WorldEnd - WorldStart;
		Segment.CollisionWorldTransform = FTransform(FRotationMatrix::MakeFromX(WorldDelta.GetSafeNormal()).ToQuat(), (WorldStart + WorldEnd) * 0.5f);
		Segment.CollisionBoxExtent = FVector(WorldDelta.Length() * 0.5f, CollisionRadius, CollisionRadius);
	}
}

FP48BridgeRopeSegment UP48BridgeRopePathComponent::BuildStraightSegment(const FVector& Start, const FVector& End, const float CollisionRadius) const
{
	FP48BridgeRopeSegment Segment;
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return Segment;
	}
	const FTransform OwnerTransform = Owner->GetActorTransform();
	Segment.Start = OwnerTransform.InverseTransformPosition(Start);
	Segment.End = OwnerTransform.InverseTransformPosition(End);
	Segment.StartTangent = Segment.End - Segment.Start;
	Segment.EndTangent = Segment.StartTangent;
	const FVector WorldDelta = End - Start;
	Segment.CollisionWorldTransform = FTransform(FRotationMatrix::MakeFromX(WorldDelta.GetSafeNormal()).ToQuat(), (Start + End) * 0.5f);
	Segment.CollisionBoxExtent = FVector(WorldDelta.Length() * 0.5f, CollisionRadius, CollisionRadius);
	return Segment;
}
