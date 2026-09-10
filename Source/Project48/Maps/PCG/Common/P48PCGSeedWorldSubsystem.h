#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "P48PCGGenerationTypes.h"
#include "P48PCGSeedWorldSubsystem.generated.h"

class AP48PCGSeedState;
class APlayerController;
class UPCGComponent;
struct FP48MapGenerationRequestMessage;

/** 한 월드의 PCG 생성 파이프라인과 네트워크 완료 합의를 조율합니다. */
UCLASS()
class PROJECT48_API UP48PCGSeedWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& World) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override
	{
		return Type == EWorldType::Game || Type == EWorldType::PIE;
	}

	const FP48PCGGenerationSnapshot& GetSnapshot() const { return Snapshot; }
	FP48PCGGenerationContext GetGenerationContext() const;

	void SetReplicatedState(AP48PCGSeedState* InSeedState);
	void HandleReplicatedSnapshot(const FP48PCGGenerationSnapshot& InSnapshot);
	void RequestGeneration(int32 RequiredPlayerCount, int32 Seed);
	void RegisterConsumer(UPCGComponent* Component);
	void NotifyClientGenerationComplete(APlayerController* PlayerController, int32 Revision);
	void NotifyControllerJoined(APlayerController* PlayerController);
	bool IsReadyForController(const APlayerController* PlayerController) const;

private:
	FGameplayMessageListenerHandle GenerationRequestHandle;
	TWeakObjectPtr<AP48PCGSeedState> SeedState;
	FP48PCGGenerationSnapshot Snapshot;
	TSet<TWeakObjectPtr<UPCGComponent>> Consumers;
	TSet<TWeakObjectPtr<UPCGComponent>> CompletedConsumers;
	TMap<TWeakObjectPtr<APlayerController>, int32> ReadyControllers;
	TArray<TWeakObjectPtr<UPCGComponent>> PendingComponents;
	int32 LastClientReportedRevision = 0;
	int32 LastCompletedRevision = 0;
	bool bGenerationRunning = false;

	void HandleGenerationRequested(FGameplayTag Channel, const FP48MapGenerationRequestMessage& Message);
	void BeginGeneration();
	void DiscoverConsumers();
	void CleanupGraphs();
	void GenerateGraphs();
	void HandleGraphGenerated(UPCGComponent* Component);
	void HandlePlayerStartReadinessChanged(int32 Revision);
	void EvaluateCompletion();
	void CompleteGeneration(bool bSucceeded);
	void SetPhase(EP48PCGGenerationPhase Phase);
	bool AreServerGraphsComplete() const;
	int32 GetReadyControllerCount() const;
};
