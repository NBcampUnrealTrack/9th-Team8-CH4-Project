#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "Project48/Items/Spawn/P48ItemSpawnPoint.h"
#include "P48PCGItemSpawnerSettings.generated.h"

/**
 * P48 Item Spawn Point 출력의 Attribute를 읽어 포인트마다 AP48ItemSpawnPoint를 생성합니다.
 * 생성할 무기 클래스와 가중치는 이 노드에서 설정합니다.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), meta = (Keywords = "item weapon spawn actor attribute sky island"))
class PROJECT48_API UP48PCGItemSpawnerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UP48PCGItemSpawnerSettings();

	/** 생성할 수 있는 무기 클래스와 선택 가중치입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapons", meta = (TitleProperty = "WeaponClass"))
	TArray<FP48WeightedWeaponSpawnEntry> SpawnEntries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnHeight = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn")
	bool bAutoSpawn = true;

	/** 같은 맵 시드에서도 이 노드의 선택 결과만 바꿀 때 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn")
	int32 SeedOffset = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Debug")
	bool bLogDiagnostics = true;

	/** P48 Item Spawn Point에서 자동으로 읽는 Attribute 이름입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Attributes")
	FName IslandIndexAttribute;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Attributes")
	FName SpawnSlotIndexAttribute;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Attributes")
	FName GenerationIdAttribute;

	virtual bool UseSeed() const override { return false; }

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("P48ItemSpawner"); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class FP48PCGItemSpawnerElement final : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* Settings) const override { return false; }
};
