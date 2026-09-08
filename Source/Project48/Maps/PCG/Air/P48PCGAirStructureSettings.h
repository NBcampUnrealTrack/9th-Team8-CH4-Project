#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "../../Datas/Structs/P48MapSpawnTypes.h"
#include "../../Datas/Structs/P48AirStructureGenerationSettings.h"
#include "P48PCGAirStructureSettings.generated.h"

/** Static Mesh와 Actor BP를 선택해 연결 가능한 하늘섬 레이아웃을 생성합니다. */
UCLASS(BlueprintType, ClassGroup = (Procedural), meta = (Keywords = "air sky island structure weighted placement"))
class PROJECT48_API UP48PCGAirStructureSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air", meta = (PCG_Overridable, TitleProperty = "Mesh"))
	TArray<FP48WeightedStaticMeshSpawn> StaticMeshes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air", meta = (PCG_Overridable, TitleProperty = "ActorClass")) 
	TArray<FP48WeightedActorSpawn> ActorClasses;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Air", meta = (PCG_Overridable))
	FP48AirStructureGenerationSettings GenerationSettings;

	virtual bool UseSeed() const override { return true; }

	static const FName StaticMeshOutputLabel;
	static const FName ActorOutputLabel;
	static const FName AnchorOutputLabel;
	static const FName MapBoundsOutputLabel;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("P48AirStructureGenerator")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class PROJECT48_API FP48PCGAirStructureElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
