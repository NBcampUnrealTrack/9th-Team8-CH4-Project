#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "P48PCGBridgeSurfacePointsSettings.generated.h"

/** 생성된 섬의 윗면에서 다리 설치 후보만 출력합니다. 액터는 생성하지 않습니다. */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PROJECT48_API UP48PCGBridgeSurfacePointsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	/** 재생성할 때 섬별 표면 검사 결과를 Output Log에 기록합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Debug")
	bool bLogDiagnostics = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Surface", meta = (ClampMin = "4", ClampMax = "64"))
	int32 DirectionCount = 16;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Surface", meta = (ClampMin = "4", ClampMax = "64"))
	int32 RadialSteps = 24;
	/** 탐색된 가장자리에서 섬 중심 방향으로 물러나는 거리입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Surface", meta = (ClampMin = "0.0", Units = "cm"))
	float EdgeInset = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Surface", meta = (ClampMin = "0.0", ClampMax = "80.0", Units = "deg"))
	float MaxSurfaceSlope = 30.0f;
	/** 디버그 포인트 Bounds의 반크기입니다. 표면 위치 자체는 변경하지 않습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Debug", meta = (ClampMin = "1.0", Units = "cm"))
	float PointExtent = 25.0f;

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("P48BridgeSurfacePoints"); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

struct FP48PCGBridgeSurfacePointsContext : public FPCGContext
{
	uint64 CollisionWaitStartFrame = 0;
	bool bCollisionWaitStarted = false;
};

class FP48PCGBridgeSurfacePointsElement : public IPCGElementWithCustomContext<FP48PCGBridgeSurfacePointsContext>
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* Settings) const override { return false; }
};
