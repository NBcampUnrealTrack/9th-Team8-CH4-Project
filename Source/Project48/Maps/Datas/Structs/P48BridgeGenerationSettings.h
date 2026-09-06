#pragma once

#include "CoreMinimal.h"
#include "P48BridgeConnectionRules.h"
#include "P48MapSpawnTypes.h"
#include "P48BridgeGenerationSettings.generated.h"

/** 연결 가능한 섬들 사이에 MST 기반 다리를 만드는 규칙입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48BridgeGenerationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge") TArray<FP48WeightedBridgeStaticMesh> StaticMeshes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge") TArray<FP48WeightedBridgeActor> ActorClasses;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge") FP48BridgeConnectionRules ConnectionRules;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge", meta = (ClampMin = "0.0", Units = "cm")) float AnchorInset = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge", meta = (ClampMin = "0.0", ClampMax = "1.0")) float AdditionalBridgeChance = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge") int32 RandomSeed = 54321;
};
