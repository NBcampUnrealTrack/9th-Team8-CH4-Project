#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P48DeathFloor.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

/** 서버에서 플레이어의 낙사 영역 진입을 감지하는 PCG 스폰용 볼륨입니다. */
UCLASS(Blueprintable)
class PROJECT48_API AP48DeathFloor : public AActor
{
	GENERATED_BODY()

public:
	AP48DeathFloor();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Death Floor")
	TObjectPtr<UBoxComponent> DeathCollision;

	UFUNCTION()
	void OnDeathCollisionBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
};
