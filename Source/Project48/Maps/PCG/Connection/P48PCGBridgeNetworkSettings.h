#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "../../Datas/Structs/P48BridgeGenerationSettings.h"
#include "P48PCGBridgeNetworkSettings.generated.h"

/** 표면 후보를 섬별로 묶거나 기존 Connection Anchor를 받아 Kruskal MST로 연결합니다. */
UCLASS(BlueprintType, ClassGroup = (Procedural), meta = (Keywords = "bridge mst connection sky island"))
class PROJECT48_API UP48PCGBridgeNetworkSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Bridge", meta = (PCG_Overridable))
	FP48BridgeGenerationSettings GenerationSettings;

	virtual bool UseSeed() const override { return true; }

	static const FName StaticMeshOutputLabel;
	static const FName ActorOutputLabel;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("P48BridgeNetworkGenerator")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class PROJECT48_API FP48PCGBridgeNetworkElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
