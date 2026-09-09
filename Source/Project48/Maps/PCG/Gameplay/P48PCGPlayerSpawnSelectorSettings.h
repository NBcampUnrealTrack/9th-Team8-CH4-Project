#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "../../Datas/Structs/P48PlayerSpawnSettings.h"
#include "P48PCGPlayerSpawnSelectorSettings.generated.h"

/** 안전한 표면 후보에서 서로 다른 섬과 수평 간격을 우선해 PlayerStart 위치를 선택합니다. */
UCLASS(BlueprintType, ClassGroup = (Procedural), meta = (Keywords = "player spawn selector sky island"))
class PROJECT48_API UP48PCGPlayerSpawnSelectorSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|PlayerSpawn", meta = (PCG_Overridable))
	FP48PlayerSpawnSettings SelectionSettings;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Map|Debug")
	bool bOverridePlayerCountForDebug = false;

	UPROPERTY(EditAnywhere, Category = "Map|Debug", meta = (EditCondition = "bOverridePlayerCountForDebug", ClampMin = "1", UIMin = "1"))
	int32 DebugPlayerCount = 1;
#endif

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
