#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P48ItemSpawnPoint.generated.h"

class AP48WeaponBase;
class UArrowComponent;

USTRUCT(BlueprintType)
struct PROJECT48_API FP48WeightedWeaponSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn")
	TSubclassOf<AP48WeaponBase> WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
};

/** PCG가 섬의 안전한 중앙 표면에 배치하는 서버 전용 아이템 스폰 지점입니다. */
UCLASS(Blueprintable)
class PROJECT48_API AP48ItemSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AP48ItemSpawnPoint();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn", meta = (ExposeOnSpawn = "true"))
	int32 SpawnSlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn", meta = (ExposeOnSpawn = "true"))
	int32 IslandIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn", meta = (ExposeOnSpawn = "true"))
	int32 GenerationId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn", meta = (TitleProperty = "WeaponClass"))
	TArray<FP48WeightedWeaponSpawnEntry> SpawnEntries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnHeight = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn")
	bool bAutoSpawn = true;

	/** 같은 맵 시드에서도 이 포인트의 선택 결과만 바꿀 때 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn")
	int32 SeedOffset = 0;

	UFUNCTION(BlueprintCallable, Category = "Item|Spawn")
	AP48WeaponBase* SpawnItem();

	UFUNCTION(BlueprintPure, Category = "Item|Spawn")
	FTransform GetItemSpawnTransform() const;

	TSubclassOf<AP48WeaponBase> SelectWeaponClass(int32 MapSeed) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Item|Spawn")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Item|Spawn")
	TObjectPtr<UArrowComponent> ArrowComponent;

	bool bRegisteredWithSubsystem = false;

	void RegisterWithSubsystem();
};
