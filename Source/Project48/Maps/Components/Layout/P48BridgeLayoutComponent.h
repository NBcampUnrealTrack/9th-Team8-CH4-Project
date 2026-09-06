#pragma once

#include "CoreMinimal.h"
#include "../../Datas/Structs/P48SwingBridgeSettings.h"
#include "Components/ActorComponent.h"
#include "P48BridgeLayoutComponent.generated.h"

struct FP48MeasuredMeshBounds;

/** 계산 컴포넌트가 액터에 전달하는 완성된 초기 배치 값입니다. */
struct FP48BridgeLayoutResult
{
	TArray<FP48BridgePlankNode> Nodes;
	TArray<FTransform> PlankTransforms;
	TArray<FTransform> LeftPlankLashingTransforms;
	TArray<FTransform> RightPlankLashingTransforms;
	TArray<FTransform> AnchorPostTransforms;
	TArray<FTransform> UpperPostKnotTransforms;
	TArray<FTransform> LowerPostKnotTransforms;

	void Reset() { Nodes.Reset(); PlankTransforms.Reset(); LeftPlankLashingTransforms.Reset(); RightPlankLashingTransforms.Reset(); AnchorPostTransforms.Reset(); UpperPostKnotTransforms.Reset(); LowerPostKnotTransforms.Reset(); }
	bool IsValid() const { return !Nodes.IsEmpty() && Nodes.Num() == PlankTransforms.Num(); }
};

/** 다리 끝점 보정과 판자 배치에 필요한 모든 공간 계산을 담당합니다. */
UCLASS(ClassGroup = (P48), meta = (BlueprintSpawnableComponent))
class PROJECT48_API UP48BridgeLayoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UP48BridgeLayoutComponent();

	bool CalculateLayout(const FVector& StartLocation, const FVector& EndLocation, const FP48SwingBridgeSettings& Settings, const FP48MeasuredMeshBounds& MeasuredBounds, FP48BridgeLayoutResult& OutResult) const;
	void CalculatePlankTransforms(const TArray<FP48BridgePlankNode>& Nodes, const FP48SwingBridgeSettings& Settings, TArray<FTransform>& OutTransforms) const;
	void CalculateAttachedTransforms(const TArray<FTransform>& BaseTransforms, const FP48BridgeAttachedMeshAssetSettings& AssetSettings, TArray<FTransform>& OutTransforms) const;

private:
	FTransform CalculatePlankTransform(const TArray<FP48BridgePlankNode>& Nodes, int32 Index, const FP48SwingBridgeSettings& Settings) const;
	FTransform CalculateAttachedTransform(const FTransform& BaseTransform, const FP48BridgeAttachedMeshAssetSettings& AssetSettings) const;
	FTransform AlignMeshBottomToWorldHeight(const FTransform& MeshTransform, const FP48BridgeAttachedMeshAssetSettings& AssetSettings, float WorldHeight) const;
	FTransform AlignMeshCenterToWorldHeight(const FTransform& MeshTransform, const FP48BridgeAttachedMeshAssetSettings& AssetSettings, float WorldHeight) const;
};
