#pragma once

#include "CoreMinimal.h"
#include "../../Datas/Structs/P48BridgeNetworkState.h"
#include "Components/ActorComponent.h"
#include "P48BridgeNetworkSyncComponent.generated.h"

struct FP48BridgePlankNode;

/** 서버 노드를 압축하고 클라이언트에서 Keyframe 사이의 시각 노드를 계산합니다. */
UCLASS(ClassGroup = (P48), meta = (BlueprintSpawnableComponent))
class PROJECT48_API UP48BridgeNetworkSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UP48BridgeNetworkSyncComponent();

	void ResetInterpolation();
	void BuildNetworkState(const TArray<FP48BridgePlankNode>& Nodes, int32 GenerationId, uint16 SimulationFrame, FP48BridgeNetworkState& OutState) const;
	void ReceiveNetworkState(const FP48BridgeNetworkState& State);
	bool CalculateClientNodes(float DeltaSeconds, const TArray<FP48BridgePlankNode>& RestNodes, TArray<FP48BridgePlankNode>& OutNodes);

private:
	float GetServerStateAge() const;

	TArray<FVector> TargetLeftOffsets;
	TArray<FVector> TargetRightOffsets;
	TArray<FVector> CurrentLeftOffsets;
	TArray<FVector> CurrentRightOffsets;
	TArray<FVector> TargetLeftVelocities;
	TArray<FVector> TargetRightVelocities;

	float LastServerTimeSeconds = 0.0f;
	float LocalReceiveTimeSeconds = 0.0f;

	int32 ActiveGenerationId = INDEX_NONE;
	uint16 LastSimulationFrame = 0;
	int32 SourceNodeCount = 0;
	bool bHasNetworkState = false;
};
