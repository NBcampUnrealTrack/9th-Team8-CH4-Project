#include "P48BridgeLayoutComponent.h"

#include "../Measurement/P48MeshBoundsComponent.h"
#include "../../Datas/Structs/P48SwingBridgeSettings.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"

UP48BridgeLayoutComponent::UP48BridgeLayoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UP48BridgeLayoutComponent::CalculateLayout(const FVector& StartLocation, const FVector& EndLocation, const FP48SwingBridgeSettings& Settings, const FP48MeasuredMeshBounds& MeasuredBounds, FP48BridgeLayoutResult& OutResult) const
{
	OutResult.Reset();
	if (!MeasuredBounds.IsValid())
	{
		return false;
	}

	const float PlankLength = Settings.Assets.Plank.ForwardAxis == EP48BridgeMeshAxis::X ? MeasuredBounds.ScaledSize.X : MeasuredBounds.ScaledSize.Y;
	const float PlankWidth = Settings.Assets.Plank.ForwardAxis == EP48BridgeMeshAxis::X ? MeasuredBounds.ScaledSize.Y : MeasuredBounds.ScaledSize.X;
	const FVector Direction = (EndLocation - StartLocation).GetSafeNormal();
	const FVector Horizontal = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
	if (Horizontal.IsNearlyZero() || PlankLength <= UE_KINDA_SMALL_NUMBER) { return false; }
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Horizontal);
	FVector DeckStart = StartLocation;
	FVector DeckEnd = EndLocation;
	if (Settings.PostConnection.bUsePostSockets)
	{
		const UStaticMesh* PostMesh = Settings.Assets.AnchorPost.Mesh;
		const UStaticMeshSocket* LowerSocket = PostMesh ? PostMesh->FindSocket(Settings.PostConnection.LowerSocketName) : nullptr;
		if (!PostMesh || !LowerSocket || !PostMesh->FindSocket(Settings.PostConnection.UpperSocketName)) { return false; }
		const float Clearance = FMath::Max(0.0f, Settings.PostConnection.PostClearance);
		auto ProjectExtent = [](const FBoxSphereBounds& Bounds, const FTransform& Transform, const FVector& Axis)
		{
			const FVector E = Bounds.BoxExtent * Transform.GetScale3D().GetAbs();
			return FMath::Abs(FVector::DotProduct(Axis, Transform.GetUnitAxis(EAxis::X))) * E.X + FMath::Abs(FVector::DotProduct(Axis, Transform.GetUnitAxis(EAxis::Y))) * E.Y + FMath::Abs(FVector::DotProduct(Axis, Transform.GetUnitAxis(EAxis::Z))) * E.Z;
		};
		// 기둥은 판자와 독립적인 시작/끝 기준점에 먼저 고정합니다.
		for (int32 EndIndex = 0; EndIndex < 2; ++EndIndex)
		{
			const FVector Anchor = EndIndex == 0 ? StartLocation : EndLocation;
			const FVector Inward = EndIndex == 0 ? Horizontal : -Horizontal;
			double Inset = 0.0;
			for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
			{
				const FVector Side = SideIndex == 0 ? -Right : Right;
				const FTransform Frame(FRotator(0.0f, Inward.Rotation().Yaw, 0.0f), Anchor + Side * (PlankWidth * 0.5f));
				FTransform Post = CalculateAttachedTransform(Frame, Settings.Assets.AnchorPost);
				const FBoxSphereBounds Bounds = PostMesh->GetBounds();
				const double InnerSide = FVector::DotProduct(Side, Post.TransformPosition(Bounds.Origin) - Anchor) - ProjectExtent(Bounds, Post, Side);
				Post.AddToTranslation(Side * FMath::Max(0.0, PlankWidth * 0.5 + Clearance - InnerSide));
				// 바닥 피벗 대신 하단 소켓을 판자 연결 높이에 맞춥니다.
				Post.AddToTranslation(FVector::UpVector * (Anchor.Z + Settings.Assets.AnchorPost.LocationOffset.Z - Post.TransformPosition(LowerSocket->RelativeLocation).Z));
				OutResult.AnchorPostTransforms.Add(Post);
				Inset = FMath::Max(Inset, FVector::DotProduct(Inward, Post.TransformPosition(Bounds.Origin) - Anchor) + ProjectExtent(Bounds, Post, Inward) + Clearance);
			}
			const FVector DeckBoundary = Anchor + (EndIndex == 0 ? Direction : -Direction) * (Inset / FVector::DotProduct(Direction, Horizontal));
			if (EndIndex == 0) { DeckStart = DeckBoundary; } else { DeckEnd = DeckBoundary; }
		}
	}
	const FVector Difference = DeckEnd - DeckStart;
	const float FullLength = FVector::DotProduct(Difference, Direction);
	const float UsableLength = FullLength - FMath::Max(0.0f, Settings.Layout.EndInset) * 2.0f;
	const float Gap = FMath::Max(0.0f, Settings.Layout.PlankGap);
	if (UsableLength < PlankLength * 2.0f + Gap) { OutResult.Reset(); return false; }
	const FVector Forward = Direction;
	const FVector UsableStart = DeckStart + Forward * FMath::Max(0.0f, Settings.Layout.EndInset);
	const float DesiredSpacing = PlankLength + Gap;
	const int32 PlankCount = FMath::Max(2, FMath::FloorToInt((UsableLength + Gap) / FMath::Max(1.0f, DesiredSpacing)));
	const float OccupiedLength = PlankLength * PlankCount + Gap * (PlankCount - 1);
	const float StartMargin = (UsableLength - OccupiedLength) * 0.5f;
	const FVector FirstCenter = UsableStart + Forward * (StartMargin + PlankLength * 0.5f);
	OutResult.Nodes.Reserve(PlankCount);
	OutResult.PlankTransforms.Reserve(PlankCount);

	for (int32 Index = 0; Index < PlankCount; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / static_cast<float>(PlankCount - 1);
		const float Sag = -4.0f * Settings.Layout.InitialSag * Alpha * (1.0f - Alpha);
		const FVector Center = FirstCenter + Forward * (DesiredSpacing * Index) + FVector::UpVector * Sag;
		FP48BridgePlankNode& Node = OutResult.Nodes.Emplace_GetRef();
		Node.RestLeft = Center - Right * (PlankWidth * 0.5f);
		Node.RestRight = Center + Right * (PlankWidth * 0.5f);
		Node.CurrentLeft = Node.RestLeft;
		Node.CurrentRight = Node.RestRight;
		Node.PreviousLeft = Node.RestLeft;
		Node.PreviousRight = Node.RestRight;
		Node.bFixed = Index == 0 || Index == PlankCount - 1;
	}

	CalculatePlankTransforms(OutResult.Nodes, Settings, OutResult.PlankTransforms);
	CalculateAttachedTransforms(OutResult.PlankTransforms, Settings.Assets.LeftPlankLashing, OutResult.LeftPlankLashingTransforms);
	CalculateAttachedTransforms(OutResult.PlankTransforms, Settings.Assets.RightPlankLashing, OutResult.RightPlankLashingTransforms);
	if (Settings.PostConnection.bUsePostSockets) { return OutResult.IsValid(); }
	for (const int32 EndIndex : { 0, OutResult.Nodes.Num() - 1 })
	{
		FTransform EndFrame = OutResult.PlankTransforms[EndIndex];
		EndFrame.SetScale3D(FVector::OneVector);
		for (const FVector& SideLocation : { OutResult.Nodes[EndIndex].CurrentLeft, OutResult.Nodes[EndIndex].CurrentRight })
		{
			FTransform PostFrame = EndFrame;
			PostFrame.SetLocation(SideLocation);
			const FTransform PostTransform = CalculateAttachedTransform(PostFrame, Settings.Assets.AnchorPost);
			OutResult.AnchorPostTransforms.Add(AlignMeshBottomToWorldHeight(PostTransform, Settings.Assets.AnchorPost, SideLocation.Z));

			FTransform UpperKnotFrame = PostFrame;
			UpperKnotFrame.SetLocation(SideLocation + FVector::UpVector * Settings.Layout.MainRopeHeight);
			const FTransform UpperKnotTransform = CalculateAttachedTransform(UpperKnotFrame, Settings.Assets.UpperPostKnot);
			OutResult.UpperPostKnotTransforms.Add(AlignMeshCenterToWorldHeight(UpperKnotTransform, Settings.Assets.UpperPostKnot, UpperKnotFrame.GetLocation().Z));

			const FTransform LowerKnotTransform = CalculateAttachedTransform(PostFrame, Settings.Assets.LowerPostKnot);
			OutResult.LowerPostKnotTransforms.Add(AlignMeshCenterToWorldHeight(LowerKnotTransform, Settings.Assets.LowerPostKnot, SideLocation.Z));
		}
	}
	return OutResult.IsValid();
}

void UP48BridgeLayoutComponent::CalculatePlankTransforms(const TArray<FP48BridgePlankNode>& Nodes, const FP48SwingBridgeSettings& Settings, TArray<FTransform>& OutTransforms) const
{
	OutTransforms.Reset(Nodes.Num());
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		OutTransforms.Add(CalculatePlankTransform(Nodes, Index, Settings));
	}
}

void UP48BridgeLayoutComponent::CalculateAttachedTransforms(const TArray<FTransform>& BaseTransforms, const FP48BridgeAttachedMeshAssetSettings& AssetSettings, TArray<FTransform>& OutTransforms) const
{
	OutTransforms.Reset(BaseTransforms.Num());
	for (const FTransform& BaseTransform : BaseTransforms)
	{
		OutTransforms.Add(CalculateAttachedTransform(BaseTransform, AssetSettings));
	}
}

FTransform UP48BridgeLayoutComponent::CalculatePlankTransform(const TArray<FP48BridgePlankNode>& Nodes, const int32 Index, const FP48SwingBridgeSettings& Settings) const
{
	if (!Nodes.IsValidIndex(Index))
	{
		return FTransform::Identity;
	}
	const FVector Center = (Nodes[Index].CurrentLeft + Nodes[Index].CurrentRight) * 0.5f;
	const FVector WidthDirection = (Nodes[Index].CurrentRight - Nodes[Index].CurrentLeft).GetSafeNormal();
	const int32 PreviousIndex = FMath::Max(0, Index - 1);
	const int32 NextIndex = FMath::Min(Nodes.Num() - 1, Index + 1);
	const FVector PreviousCenter = (Nodes[PreviousIndex].CurrentLeft + Nodes[PreviousIndex].CurrentRight) * 0.5f;
	const FVector NextCenter = (Nodes[NextIndex].CurrentLeft + Nodes[NextIndex].CurrentRight) * 0.5f;
	const FVector ForwardDirection = (NextCenter - PreviousCenter).GetSafeNormal();
	const FQuat Rotation = Settings.Assets.Plank.ForwardAxis == EP48BridgeMeshAxis::X ? FRotationMatrix::MakeFromXY(ForwardDirection, WidthDirection).ToQuat() : FRotationMatrix::MakeFromXY(WidthDirection, ForwardDirection).ToQuat();
	const FVector MeshCenter = Settings.Assets.Plank.Mesh ? Settings.Assets.Plank.Mesh->GetBounds().Origin : FVector::ZeroVector;
	return FTransform(Rotation, Center - Rotation.RotateVector(MeshCenter * Settings.Assets.Plank.Scale), Settings.Assets.Plank.Scale);
}

FTransform UP48BridgeLayoutComponent::CalculateAttachedTransform(const FTransform& BaseTransform, const FP48BridgeAttachedMeshAssetSettings& AssetSettings) const
{
	FTransform ParentTransform = BaseTransform;
	ParentTransform.SetScale3D(FVector::OneVector);
	const FTransform LocalOffset(AssetSettings.RotationOffset, AssetSettings.LocationOffset, AssetSettings.Scale);
	return LocalOffset * ParentTransform;
}

FTransform UP48BridgeLayoutComponent::AlignMeshBottomToWorldHeight(const FTransform& MeshTransform, const FP48BridgeAttachedMeshAssetSettings& AssetSettings, const float WorldHeight) const
{
	if (!AssetSettings.Mesh)
	{
		return MeshTransform;
	}
	FTransform Result = MeshTransform;
	const FBox WorldBounds = AssetSettings.Mesh->GetBounds().TransformBy(Result).GetBox();
	Result.AddToTranslation(FVector::UpVector * (WorldHeight + AssetSettings.LocationOffset.Z - WorldBounds.Min.Z));
	return Result;
}

FTransform UP48BridgeLayoutComponent::AlignMeshCenterToWorldHeight(const FTransform& MeshTransform, const FP48BridgeAttachedMeshAssetSettings& AssetSettings, const float WorldHeight) const
{
	if (!AssetSettings.Mesh)
	{
		return MeshTransform;
	}
	FTransform Result = MeshTransform;
	const FBoxSphereBounds WorldBounds = AssetSettings.Mesh->GetBounds().TransformBy(Result);
	Result.AddToTranslation(FVector::UpVector * (WorldHeight + AssetSettings.LocationOffset.Z - WorldBounds.Origin.Z));
	return Result;
}
