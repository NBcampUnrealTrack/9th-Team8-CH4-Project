#pragma once

#include "CoreMinimal.h"
#include "P48DeathVolumeGenerationSettings.generated.h"

/** PCG가 생성하는 사망 판정 볼륨의 크기와 높이 설정입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48DeathVolumeGenerationSettings
{
	GENERATED_BODY()

	/** 생성 범위에 곱할 XY 배율입니다. 1보다 크게 두면 맵이 커질 때 안전 영역도 비례해 커집니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Death Volume", meta = (ClampMin = "1.0"))
	float CoverageScale = 1.25f;

	/** 입력 Bounds의 각 XY 가장자리에 추가할 여유 거리입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Death Volume", meta = (ClampMin = "0.0", Units = "cm"))
	float HorizontalPadding = 1000.0f;

	/** 입력 Bounds가 실제 생성 맵보다 작더라도 보장할 사망 볼륨의 최소 XY 전체 크기입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Death Volume", meta = (ClampMin = "1.0", Units = "cm"))
	FVector2D MinimumHorizontalSize = FVector2D(50000.0f, 50000.0f);

	/** 입력 Bounds의 최저점 아래로 생성할 사망 볼륨의 두께입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Death Volume", meta = (ClampMin = "1.0", Units = "cm"))
	float Depth = 1000.0f;

	/** 입력 Bounds 최저점에서 사망 볼륨 상단까지의 간격을 Depth 배수로 지정합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Death Volume", meta = (ClampMin = "0.0"))
	float VerticalOffsetDepthMultiplier = 2.0f;
};
