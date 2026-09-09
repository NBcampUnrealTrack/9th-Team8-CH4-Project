#pragma once

#include "CoreMinimal.h"
#include "P48BridgeConnectionRules.h"
#include "P48AirStructureGenerationSettings.generated.h"

UENUM(BlueprintType)
enum class EP48AirPlacementMode : uint8
{
	Free UMETA(DisplayName = "Free Air Placement"),
	SkyIsland UMETA(DisplayName = "Sky Island Rules")
};

/** 일반 공중 배치와 연결 가능한 하늘섬 배치를 선택해서 사용하는 규칙입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48AirStructureGenerationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air")
	EP48AirPlacementMode PlacementMode = EP48AirPlacementMode::SkyIsland;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air", meta = (ClampMin = "1"))
	int32 TargetCount = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air")
	int32 RandomSeed = 12345;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air", meta = (ClampMin = "0.0", Units = "cm"))
	FVector2D MapSize = FVector2D(40000.0f, 40000.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air", meta = (Units = "cm"))
	FVector2D HeightRange = FVector2D(-1000.0f, 1000.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air", meta = (ClampMin = "0.0", Units = "cm"))
	float GlobalMinGap = 500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air", meta = (ClampMin = "1"))
	int32 MaxPlacementAttemptsPerStructure = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air", meta = (ClampMin = "1"))
	int32 MaxMapGenerationAttempts = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air|SkyIsland", meta = (EditCondition = "PlacementMode == EP48AirPlacementMode::SkyIsland"))
	bool bRequireConnectableLayout = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air|SkyIsland", meta = (EditCondition = "PlacementMode == EP48AirPlacementMode::SkyIsland", ClampMin = "1.0", Units = "cm"))
	float MaxIslandCenterDistance = 5000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air|SkyIsland", meta = (EditCondition = "PlacementMode == EP48AirPlacementMode::SkyIsland", ClampMin = "0.0", Units = "cm"))
	float HeightStep = 250.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air|SkyIsland", meta = (EditCondition = "PlacementMode == EP48AirPlacementMode::SkyIsland && bRequireConnectableLayout"))
	FP48BridgeConnectionRules ConnectionRules;
};
