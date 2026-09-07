#pragma once

#include "CoreMinimal.h"
#include "../../Datas/Structs/P48BridgeNetworkState.h"
#include "../../Datas/Structs/P48SwingBridgeSettings.h"
#include "GameFramework/Actor.h"
#include "P48SwingBridge.generated.h"

class USceneComponent;
class USplineComponent;
class USplineMeshComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UP48BridgeLayoutComponent;
class UP48BridgeLoadComponent;
class UP48BridgeNetworkSyncComponent;
class UP48BridgePhysicsComponent;
class UP48BridgeRopePathComponent;
class UP48MeshBoundsComponent;

struct FP48BridgeLayoutResult;
struct FP48BridgeAttachedMeshAssetSettings;
struct FP48BridgeRopeAssetSettings;
struct FP48BridgeRopePathResult;
struct FP48BridgeRopeSegment;

/** 두 끝점 사이에 메시 크기를 기준으로 판자를 자동 배치하는 흔들다리 Actor입니다. */
UCLASS(Blueprintable)
class PROJECT48_API AP48SwingBridge : public AActor
{
	GENERATED_BODY()

public:
	AP48SwingBridge();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_BridgeDefinition, Category = "Bridge|Placement", meta = (Units = "cm"))
	FVector StartLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_BridgeDefinition, Category = "Bridge|Placement", meta = (Units = "cm"))
	FVector EndLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	FP48SwingBridgeSettings Settings;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Bridge")
	void RebuildBridge();
	UFUNCTION(BlueprintCallable, Category = "Bridge|Physics")
	void ApplyBridgeImpulseAtLocation(FVector WorldLocation, FVector WorldImpulse);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_BridgeDefinition();
	UFUNCTION()
	void OnRep_BridgeNetworkState();
	void ClearGeneratedComponents();
	void PlacePlanks(const FP48BridgeLayoutResult& LayoutResult);
	void PlaceAttachedMeshes(const FP48BridgeAttachedMeshAssetSettings& AssetSettings, const TArray<FTransform>& Transforms, const TCHAR* NamePrefix, TArray<TObjectPtr<UStaticMeshComponent>>& OutComponents, bool bAllowCollision = true);
	void PlaceRopes(const FP48BridgeRopePathResult& RopeResult);
	void TickServerSimulation(float DeltaSeconds);
	void TickClientInterpolation(float DeltaSeconds);
	void ApplyNodeState(const TArray<FP48BridgePlankNode>& Nodes, bool bUpdateRopes, bool bApplyPlanks = true);
	void PublishNetworkState();
	int32 ResolveGenerationId() const;
	void ApplyPlankTransforms(const TArray<FTransform>& PlankTransforms);
	void ApplyRopePaths(const FP48BridgeRopePathResult& RopeResult, bool bUpdateCollision);
	bool IsPlayerNearBridge() const;
	void ApplySplinePoints(USplineComponent* Spline, const TArray<FVector>& Points);
	void ApplyRopeSegments(const TArray<FP48BridgeRopeSegment>& Values, const TArray<TObjectPtr<USplineMeshComponent>>& Components);
	void CreateSplineMeshComponents(int32 Count, const TCHAR* NamePrefix, const FP48BridgeRopeAssetSettings& RopeAssets, TArray<TObjectPtr<USplineMeshComponent>>& OutComponents);
	void CreateRopeCollisionComponents(int32 Count);
	void ApplyRopeCollision(const FP48BridgeRopePathResult& RopeResult);

	UPROPERTY(VisibleAnywhere, Category = "Bridge|Component")
	TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category = "Bridge|Component")
	TObjectPtr<USplineComponent> LeftMainRopeSpline;
	UPROPERTY(VisibleAnywhere, Category = "Bridge|Component")
	TObjectPtr<USplineComponent> RightMainRopeSpline;
	UPROPERTY(VisibleAnywhere, Category = "Bridge|Component")
	TObjectPtr<UP48MeshBoundsComponent> MeshBoundsComponent;
	UPROPERTY(VisibleAnywhere, Category = "Bridge|Component")
	TObjectPtr<UP48BridgeLayoutComponent> BridgeLayoutComponent;
	UPROPERTY(VisibleAnywhere, Category = "Bridge|Component")
	TObjectPtr<UP48BridgeLoadComponent> BridgeLoadComponent;
	UPROPERTY(VisibleAnywhere, Category = "Bridge|Component")
	TObjectPtr<UP48BridgeNetworkSyncComponent> BridgeNetworkSyncComponent;
	UPROPERTY(VisibleAnywhere, Category = "Bridge|Component")
	TObjectPtr<UP48BridgePhysicsComponent> BridgePhysicsComponent;
	UPROPERTY(VisibleAnywhere, Category = "Bridge|Component")
	TObjectPtr<UP48BridgeRopePathComponent> BridgeRopePathComponent;
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PlankComponents;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LeftPlankLashingComponents;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> RightPlankLashingComponents;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> AnchorPostComponents;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> UpperPostKnotComponents;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LowerPostKnotComponents;
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> LeftMainRopeComponents;
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> RightMainRopeComponents;
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> ConnectorRopeComponents;
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> LeftLowerRopeComponents;
	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> RightLowerRopeComponents;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> RopeCollisionComponents;
	UPROPERTY(ReplicatedUsing = OnRep_BridgeNetworkState)
	FP48BridgeNetworkState BridgeNetworkState;

	float LoadUpdateAccumulator = 0.0f;
	float NetworkUpdateAccumulator = 0.0f;
	float RopeVisualUpdateAccumulator = 0.0f;
	uint16 SimulationFrame = 0;
	int32 BridgeGenerationId = 0;
	TArray<FP48BridgePlankNode> RestNodes;
	TArray<FP48BridgePlankNode> ClientNodes;
	bool bIsRebuilding = false;
};
