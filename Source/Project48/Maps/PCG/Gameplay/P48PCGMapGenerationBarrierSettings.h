#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "P48PCGMapGenerationBarrierSettings.generated.h"

/** 모든 맵 생성 분기가 끝난 뒤 섬 Point를 PlayerStart 단계로 전달하는 실행 Barrier입니다. */
UCLASS(BlueprintType, ClassGroup = (Procedural), meta = (Keywords = "map generation barrier player start"))
class PROJECT48_API UP48PCGMapGenerationBarrierSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	static const FName IslandPointsPin;
	static const FName DependenciesPin;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("P48MapGenerationBarrier"); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class FP48PCGMapGenerationBarrierElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* Settings) const override { return false; }
};
