#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "P48PCGPlayerSpawnSurfacePointsSettings.generated.h"

/** 생성된 섬의 실제 윗면에서 안전한 PlayerStart 후보를 만듭니다. */
UCLASS(BlueprintType, ClassGroup = (Procedural), meta = (Keywords = "player spawn surface sky island"))
class PROJECT48_API UP48PCGPlayerSpawnSurfacePointsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|PlayerSpawn", meta = (ClampMin = "100.0", Units = "cm"))
	float CandidateSpacing = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|PlayerSpawn", meta = (ClampMin = "0.0", Units = "cm"))
	float EdgeSafeDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|PlayerSpawn", meta = (ClampMin = "0.0", ClampMax = "80.0", Units = "deg"))
	float MaxSurfaceSlope = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Debug")
	bool bLogDiagnostics = true;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("P48PlayerSpawnSurfacePoints"); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

struct FP48PCGPlayerSpawnSurfaceContext : public FPCGContext
{
	uint64 CollisionWaitStartFrame = 0;
	bool bCollisionWaitStarted = false;
};

class FP48PCGPlayerSpawnSurfaceElement : public IPCGElementWithCustomContext<FP48PCGPlayerSpawnSurfaceContext>
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* Settings) const override { return false; }
};
