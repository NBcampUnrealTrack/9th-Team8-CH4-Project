#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "../../Datas/Structs/P48DeathVolumeGenerationSettings.h"
#include "P48PCGDeathVolumeSettings.generated.h"

/** 입력 Spatial Bounds의 XY 크기에 맞는 사망 볼륨 Spawn Actor 포인트를 생성합니다. */
UCLASS(BlueprintType, ClassGroup = (Procedural), meta = (Keywords = "death kill floor volume bounds"))
class PROJECT48_API UP48PCGDeathVolumeSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Death Volume", meta = (PCG_Overridable))
	FP48DeathVolumeGenerationSettings GenerationSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Debug")
	bool bLogDiagnostics = true;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("P48DeathVolumePoint"); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class FP48PCGDeathVolumeElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* Settings) const override { return false; }
};
