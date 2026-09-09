#pragma once

#include "CoreMinimal.h"
#include "P48BridgeNetworkState.generated.h"

/** 서버의 판자 노드 하나를 클라이언트에 전달하기 위한 압축 상태입니다. */
USTRUCT()
struct PROJECT48_API FP48BridgeNetworkNodeState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 LeftOffset = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantize10 RightOffset = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantize10 LeftVelocity = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantize10 RightVelocity = FVector::ZeroVector;
};

/** 서버가 주기적으로 전송하는 다리 물리 상태입니다. */
USTRUCT()
struct PROJECT48_API FP48BridgeNetworkState
{
	GENERATED_BODY()

	UPROPERTY()
	int32 GenerationId = 0;

	UPROPERTY()
	uint16 SimulationFrame = 0;

	UPROPERTY()
	float ServerTimeSeconds = 0.0f;

	UPROPERTY()
	int32 SourceNodeCount = 0;

	/** 전체 판자 대신 균등 간격으로 뽑은 제어점만 복제합니다. */
	UPROPERTY()
	TArray<FP48BridgeNetworkNodeState> ControlPoints;
};
