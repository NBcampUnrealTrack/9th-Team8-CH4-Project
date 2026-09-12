#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "P48PCGItemSpawnPointSettings.generated.h"

/** 생성된 각 섬에서 중심과 가장 가까운 안전한 아이템 스폰 지점 하나를 만듭니다. */
UCLASS(BlueprintType, ClassGroup = (Procedural), meta = (Keywords = "item weapon spawn center sky island"))
class PROJECT48_API UP48PCGItemSpawnPointSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	/** 섬 중심이 막혀 있을 때 바깥쪽 후보를 검사하는 간격입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn", meta = (ClampMin = "50.0", Units = "cm"))
	float SearchStep = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxSearchRadius = 800.0f;

	/** 낭떠러지 바로 옆이 선택되지 않도록 주변 바닥을 검사합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn", meta = (ClampMin = "0.0", Units = "cm"))
	float SafeRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn", meta = (ClampMin = "0.0", ClampMax = "80.0", Units = "deg"))
	float MaxSurfaceSlope = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Spawn", meta = (ClampMin = "4", ClampMax = "32"))
	int32 SamplesPerRing = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Debug")
	bool bLogDiagnostics = true;

	virtual bool UseSeed() const override { return true; }

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("P48ItemSpawnPoint"); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

struct FP48PCGItemSpawnPointContext : public FPCGContext
{
	uint64 CollisionWaitStartFrame = 0;
	bool bCollisionWaitStarted = false;
};

class FP48PCGItemSpawnPointElement : public IPCGElementWithCustomContext<FP48PCGItemSpawnPointContext>
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* Settings) const override { return false; }
};
