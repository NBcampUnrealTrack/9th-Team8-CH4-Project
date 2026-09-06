#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "../../Datas/Structs/P48PlayerSpawnSettings.h"
#include "P48PCGPlayerSpawnSelectorSettings.generated.h"

/** 하늘섬 Connection Anchor 중 안전 간격을 만족하는 플레이어 시작점을 선택합니다. */
UCLASS(BlueprintType, ClassGroup = (Procedural), meta = (Keywords = "player spawn selector sky island"))
class PROJECT48_API UP48PCGPlayerSpawnSelectorSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|PlayerSpawn", meta = (PCG_Overridable))
	FP48PlayerSpawnSettings SelectionSettings;

	virtual bool UseSeed() const override { return true; }

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("P48PlayerSpawnSelector")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class PROJECT48_API FP48PCGPlayerSpawnSelectorElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
