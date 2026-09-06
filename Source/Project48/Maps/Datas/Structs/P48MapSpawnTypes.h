#pragma once

#include "CoreMinimal.h"
#include "P48MapSpawnTypes.generated.h"

class AActor;
class UStaticMesh;

/** PCG가 선택할 움직이지 않는 스태틱 메시 항목입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48WeightedStaticMeshSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Spawn")
	TObjectPtr<UStaticMesh> Mesh = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Spawn", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Spawn", meta = (ClampMin = "0.001", AllowPreserveRatio = "true"))
	FVector Scale = FVector::OneVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float MinGap = 500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float PlacementRadiusOverride = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Connection")
	bool bCanConnectBridge = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Connection")
	bool bOverrideBridgeAnchorHeight = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Connection", meta = (EditCondition = "bOverrideBridgeAnchorHeight", Units = "cm"))
	float BridgeAnchorHeight = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Gameplay")
	bool bCanSpawnPlayer = true;
};

/** PCG가 선택할 움직임이나 능력을 가진 Actor Blueprint 항목입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48WeightedActorSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Spawn")
	TSubclassOf<AActor> ActorClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Spawn", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Spawn", meta = (ClampMin = "0.001", AllowPreserveRatio = "true"))
	FVector Scale = FVector::OneVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Placement", meta = (ClampMin = "1.0", Units = "cm"))
	float PlacementRadius = 1000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float MinGap = 500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Connection")
	bool bCanConnectBridge = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Connection", meta = (Units = "cm"))
	float BridgeAnchorHeight = 500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Gameplay")
	bool bCanSpawnPlayer = false;
};

/** 길이에 맞춰 배치할 다리 스태틱 메시 항목입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48WeightedBridgeStaticMesh
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge")
	TObjectPtr<UStaticMesh> Mesh = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge", meta = (ClampMin = "1.0", Units = "cm"))
	float NativeLength = 1000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge", meta = (ClampMin = "0.001"))
	FVector BaseScale = FVector::OneVector;
};

/** 양 끝점 정보를 받아 동작할 다리 Actor Blueprint 항목입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48WeightedBridgeActor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge")
	TSubclassOf<AActor> ActorClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
};
