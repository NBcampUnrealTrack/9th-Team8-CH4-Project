#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "P48BridgeLoadComponent.generated.h"

class UStaticMeshComponent;

/** 물리 컴포넌트로 전달할 한 지점의 지속 하중 값입니다. */
struct FP48BridgeLoadValue
{
	FVector WorldLocation = FVector::ZeroVector;
	FVector Acceleration = FVector::ZeroVector;
};

/** 판자 위 Pawn을 찾아 위치와 하중 값만 계산합니다. */
UCLASS(ClassGroup = (P48), meta = (BlueprintSpawnableComponent))
class PROJECT48_API UP48BridgeLoadComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UP48BridgeLoadComponent();

	void CalculateStandingLoads(const TArray<TObjectPtr<UStaticMeshComponent>>& Planks, TArray<FP48BridgeLoadValue>& OutLoads) const;
};
