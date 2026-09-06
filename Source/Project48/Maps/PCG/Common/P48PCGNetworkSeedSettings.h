#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "P48PCGNetworkSeedSettings.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PROJECT48_API UP48PCGNetworkSeedSettings : public UPCGSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Seed")
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
