#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "P48BridgeRopePathComponent.generated.h"

struct FP48BridgePlankNode;
struct FP48BridgePostConnectionSettings;
class UStaticMeshComponent;

/** 액터가 SplineMesh에 그대로 적용할 한 로프 구간의 값입니다. */
struct FP48BridgeRopeSegment
{
	FVector Start = FVector::ZeroVector;
	FVector StartTangent = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	FVector EndTangent = FVector::ZeroVector;
	FTransform CollisionWorldTransform = FTransform::Identity;
	FVector CollisionBoxExtent = FVector::ZeroVector;
};

/** 좌우 메인 로프와 판자 연결 로프의 완성된 경로 값입니다. */
struct FP48BridgeRopePathResult
{
	TArray<FVector> LeftMainPoints;
	TArray<FVector> RightMainPoints;
	TArray<FP48BridgeRopeSegment> LeftMainSegments;
	TArray<FP48BridgeRopeSegment> RightMainSegments;
	TArray<FP48BridgeRopeSegment> ConnectorSegments;
	TArray<FP48BridgeRopeSegment> LeftLowerSegments;
	TArray<FP48BridgeRopeSegment> RightLowerSegments;

	void Reset() { LeftMainPoints.Reset(); RightMainPoints.Reset(); LeftMainSegments.Reset(); RightMainSegments.Reset(); ConnectorSegments.Reset(); LeftLowerSegments.Reset(); RightLowerSegments.Reset(); }
};

/** 현재 물리 노드에서 스플라인과 로프 충돌에 필요한 값만 계산합니다. */
UCLASS(ClassGroup = (P48), meta = (BlueprintSpawnableComponent))
class PROJECT48_API UP48BridgeRopePathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UP48BridgeRopePathComponent();
	bool ConfigurePostSockets(const TArray<TObjectPtr<UStaticMeshComponent>>& Posts, const FP48BridgePostConnectionSettings& Settings, FString& OutError);
	void ResetPostSockets();

	bool CalculatePaths(const TArray<FP48BridgePlankNode>& Nodes, float MainRopeHeight, float CollisionRadius, FP48BridgeRopePathResult& OutResult) const;

private:
	// 시작 좌/우, 끝 좌/우 순서의 월드 소켓 Transform입니다. 재조립할 때만 조회합니다.
	TArray<FTransform> UpperSockets;
	TArray<FTransform> LowerSockets;
	bool bGenerateLowerRopes = false;
	void BuildMainSegments(const TArray<FVector>& Points, float CollisionRadius, TArray<FP48BridgeRopeSegment>& OutSegments) const;
	FP48BridgeRopeSegment BuildStraightSegment(const FVector& Start, const FVector& End, float CollisionRadius) const;
};
