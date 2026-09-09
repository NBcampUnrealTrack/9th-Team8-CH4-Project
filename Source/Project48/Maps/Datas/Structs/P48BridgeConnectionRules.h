#pragma once

#include "CoreMinimal.h"
#include "P48BridgeConnectionRules.generated.h"

/** 하늘섬 배치와 MST 다리 생성이 함께 사용하는 연결 가능 조건입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48BridgeConnectionRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxBridgeLength = 15000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge", meta = (ClampMin = "0.0", Units = "cm"))
	float MinHorizontalDistance = 500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxHeightDifference = 5000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float MaxSlopeAngle = 25.0f;

	/** 다리 방향과 섬 가장자리의 바깥 방향이 이루어도 되는 최대 각도입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float MaxEndpointFacingAngle = 70.0f;
};
