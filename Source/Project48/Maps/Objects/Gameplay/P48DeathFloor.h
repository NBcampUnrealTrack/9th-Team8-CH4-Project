#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P48DeathFloor.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class AP48PlayerCharacter;

/** 서버에서 플레이어의 낙사 영역 진입을 감지하는 PCG 스폰용 볼륨입니다. */
UCLASS(Blueprintable)
class PROJECT48_API AP48DeathFloor : public AActor
{
	GENERATED_BODY()

public:
	AP48DeathFloor();
	virtual void Tick(float DeltaSeconds) override;

	/** 수동 배치 시 사용하는 기본 충돌 범위이며, PCG 스케일 계산의 기준이기도 합니다. */
	static const FVector DefaultCollisionExtent;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;
	void ApplyRuntimeCollisionSettings();
	void KillPlayersInsideBounds();
	void KillPlayerOnce(AP48PlayerCharacter* PlayerCharacter, const TCHAR* DetectionSource);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Death Floor")
	TObjectPtr<UBoxComponent> DeathCollision;

	/** 사망 영역의 전체 크기입니다. 에디터에서 각 인스턴스마다 설정할 수 있습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Floor", meta = (ClampMin = "1.0", Units = "cm"))
	FVector CollisionSize;

	/** 게임 실행 중 사망 영역을 빨간 박스로 계속 표시합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death Floor|Debug")
	bool bDrawDebugBounds = true;

	TSet<TWeakObjectPtr<AP48PlayerCharacter>> TickDetectedPlayers;

	UFUNCTION()
	void OnDeathCollisionBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
};
