#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "P48ItemSpawnSubsystem.generated.h"

class AP48ItemSpawnPoint;
class AP48WeaponBase;

/** 한 월드의 아이템 스폰 포인트와 생성된 아이템을 서버에서 관리합니다. */
UCLASS()
class PROJECT48_API UP48ItemSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	bool RegisterSpawnPoint(AP48ItemSpawnPoint* SpawnPoint);
	void UnregisterSpawnPoint(AP48ItemSpawnPoint* SpawnPoint);

	UFUNCTION(BlueprintCallable, Category = "Item|Spawn")
	AP48WeaponBase* SpawnAtPoint(AP48ItemSpawnPoint* SpawnPoint);

	UFUNCTION(BlueprintPure, Category = "Item|Spawn")
	AP48WeaponBase* GetSpawnedItem(const AP48ItemSpawnPoint* SpawnPoint) const;

private:
	TMap<TWeakObjectPtr<AP48ItemSpawnPoint>, TWeakObjectPtr<AP48WeaponBase>> SpawnedItems;

	int32 ResolveMapSeed() const;
	bool IsCurrentGeneration(const AP48ItemSpawnPoint* SpawnPoint) const;
};
