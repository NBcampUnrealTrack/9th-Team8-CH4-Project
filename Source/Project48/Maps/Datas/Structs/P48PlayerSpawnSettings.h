#pragma once

#include "CoreMinimal.h"
#include "P48PlayerSpawnSettings.generated.h"

/** 연결된 하늘섬 Anchor에서 플레이어 시작점을 고르는 규칙입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48PlayerSpawnSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|PlayerSpawn", meta = (ClampMin = "1"))
	int32 SpawnCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|PlayerSpawn", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnHeight = 1000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|PlayerSpawn", meta = (ClampMin = "0.0", Units = "cm"))
	float MinSpawnDistance = 3000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|PlayerSpawn")
	int32 RandomSeed = 24680;
};
