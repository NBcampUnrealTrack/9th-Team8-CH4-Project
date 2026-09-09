#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P48PCGSeedState.generated.h"

class UPCGComponent;
class APlayerController;
class AP48PlayerStart;

USTRUCT()
struct FP48PCGSeedSnapshot
{
	GENERATED_BODY()
	UPROPERTY()
	int32 Seed = 0;
	UPROPERTY()
	int32 Revision = 0;
	UPROPERTY()
	bool bMapReady = false;
};

/** Network Seed 노드가 자동 생성하는 복제 상태입니다. BP/레벨 배치 불필요. */
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class PROJECT48_API AP48PCGSeedState : public AActor
{
	GENERATED_BODY()
public:
	AP48PCGSeedState();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(Replicated)
	FP48PCGSeedSnapshot State;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map|Seed")
	void SetMapSeed(int32 Seed);
	void RegisterConsumer(UPCGComponent* Component);
	void ReportPlayerStartLayout(int32 RequestedCount, int32 SelectedCount);
	void RegisterGeneratedPlayerStart(AP48PlayerStart* PlayerStart);
	void UnregisterGeneratedPlayerStart(AP48PlayerStart* PlayerStart);
	void NotifyClientGenerationComplete(APlayerController* PlayerController, int32 Revision);
	void NotifyControllerJoined(APlayerController* PlayerController);
	bool IsReadyForController(const APlayerController* PlayerController) const;
	bool IsMapReady() const { return State.bMapReady; }
private:
	TMap<TWeakObjectPtr<UPCGComponent>, int32> Consumers;
	TMap<TWeakObjectPtr<UPCGComponent>, int32> CompletedConsumers;
	TMap<TWeakObjectPtr<APlayerController>, int32> ReadyControllers;
	TSet<TWeakObjectPtr<UPCGComponent>> BoundConsumers;
	TSet<TWeakObjectPtr<UPCGComponent>> PendingGenerationConsumers;
	TSet<TWeakObjectPtr<AP48PlayerStart>> RegisteredPlayerStarts;
	bool bServerGenerationComplete = false;
	bool bPlayerStartLayoutRequired = false;
	bool bPlayerStartLayoutValid = false;
	bool bConsumerGenerationScheduled = false;
	bool bPlayerStartTimeoutLogged = false;
	int32 RequestedPlayerStartCount = 0;
	int32 SelectedPlayerStartCount = 0;
	int32 LastReportedRevision = 0;
	double PlayerStartRegistrationDeadline = 0.0;
	void HandleGraphGenerated(UPCGComponent* Component);
	bool IsLocalGenerationComplete() const;
	int32 GetRegisteredPlayerStartCount() const;
	void RefreshPlayerStartReadiness();
	void GeneratePendingConsumers();
	void UpdateMapReady();
};
