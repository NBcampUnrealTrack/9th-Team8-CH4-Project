#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "P48PCGNetworkSeedSettings.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PROJECT48_API UP48PCGNetworkSeedSettings : public UPCGSettings
{
	GENERATED_BODY()
public:
	/** 서버에서 새 맵 생성 Revision에 사용할 비결정 Seed를 만듭니다. */
	static int32 GenerateServerSeed();

	/** 이전 에셋 호환용입니다. 공통 Seed는 PCG Component의 Seed를 사용합니다. */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use the PCG Component Seed instead."))
	int32 EditorPreviewSeed = 12345;
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("P48NetworkSeed"); }
	virtual FText GetDefaultNodeTitle() const override { return FText::FromString(TEXT("P48 Network Seed")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
#endif
protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return {}; }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class FP48PCGNetworkSeedElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext*) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings*) const override { return false; }
};
