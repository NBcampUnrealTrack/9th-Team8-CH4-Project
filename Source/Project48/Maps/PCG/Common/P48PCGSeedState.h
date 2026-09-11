#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P48PCGGenerationTypes.h"
#include "P48PCGSeedState.generated.h"

class AP48PlayerStart;
class APlayerController;
class UPCGComponent;

/** 생성 스냅샷을 복제하고 기존 호출을 담당 Subsystem으로 전달하는 얇은 네트워크 경계입니다. */
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class PROJECT48_API AP48PCGSeedState : public AActor
{
	GENERATED_BODY()

public:
	AP48PCGSeedState();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FP48PCGGenerationSnapshot State;

	void SetGenerationSnapshot(const FP48PCGGenerationSnapshot& InSnapshot);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map|Seed")
	void SetMapSeed(int32 Seed);

	void RegisterConsumer(UPCGComponent* Component);
	void ReportPlayerStartLayout(int32 RequestedCount, int32 SelectedCount);
	void RegisterGeneratedPlayerStart(AP48PlayerStart* PlayerStart);
	void UnregisterGeneratedPlayerStart(AP48PlayerStart* PlayerStart);
	void NotifyClientGenerationComplete(APlayerController* PlayerController, int32 Revision);
	void NotifyControllerJoined(APlayerController* PlayerController);
	bool IsReadyForController(const APlayerController* PlayerController) const;
	bool IsMapReady() const { return State.IsReady(); }

private:
	UFUNCTION()
	void OnRep_State();
};
