#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "P48PCGGenerationTypes.h"
#include "P48PCGSeedWorldSubsystem.generated.h"

class AP48PCGSeedState;
class APlayerController;
class UPCGComponent;
class UPCGGraph;
struct FPCGContextHandle;
struct FP48MatchPlayerCountMessage;

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
	static bool HasNetworkSeedConsumer(UWorld* World);
	FP48PCGGenerationContext GetGenerationContext() const;
	bool CanReceivePlayerCount() const { return PlayerCountChangedHandle.IsValid(); }

	void SetReplicatedState(AP48PCGSeedState* InSeedState);
	void HandleReplicatedSnapshot(const FP48PCGGenerationSnapshot& InSnapshot);
	/** 플레이어 수와 무관하게 새 Seed로 맵 생성을 시작합니다. */
	void RequestGeneration();

	/** SeedState의 기존 명시적 Seed API를 위한 Maps 내부 호환 경로입니다. */
	void RequestGenerationWithSeed(int32 Seed);

	/** 이미 생성 중이거나 생성된 맵에 플레이어 준비 정책만 갱신합니다. */
	void SetRequiredPlayerCount(int32 RequiredPlayerCount);
	/** Non-shipping server debug override. Call before selecting the PlayerStart layout. */
	void ApplyDebugPlayerCount(int32 PlayerCount);
	void RegisterPlayerCountWaiter(int32 Revision, TWeakPtr<FPCGContextHandle> ContextHandle);
	void RegisterConsumer(UPCGComponent* Component);
	void NotifyClientGenerationComplete(APlayerController* PlayerController, int32 Revision);
	void NotifyControllerJoined(APlayerController* PlayerController);
	bool IsReadyForController(const APlayerController* PlayerController) const;

private:
	FGameplayMessageListenerHandle PlayerCountChangedHandle;
	TWeakObjectPtr<AP48PCGSeedState> SeedState;
	FP48PCGGenerationSnapshot Snapshot;
	TSet<TWeakObjectPtr<UPCGComponent>> Consumers;
	TSet<TWeakObjectPtr<UPCGComponent>> CompletedConsumers;
	TMap<TWeakObjectPtr<APlayerController>, int32> ReadyControllers;
	TArray<TWeakObjectPtr<UPCGComponent>> PendingComponents;
	TArray<TPair<int32, TWeakPtr<FPCGContextHandle>>> PlayerCountWaiters;
	int32 LastCompletedRevision = 0;
	int32 ConfirmedPlayerCount = 0;
	bool bPlayerCountLocked = false;
	bool bDebugPlayerCountOverride = false;
	bool bGenerationRunning = false;
	FTimerHandle ClientReportTimer;
	void TryReportClientCompletion();

	void HandlePlayerCountChanged(FGameplayTag Channel, const FP48MatchPlayerCountMessage& Message);
	void BeginGeneration();
	void DiscoverConsumers();
	static bool GraphUsesNetworkSeed(const UPCGGraph* Graph);
	void ApplySeedToConsumers();
	void CleanupGraphs();
	void GenerateGraphs();
	void PrepareGeneratedSurfacesForNetworking(UPCGComponent* Component) const;
	void HandleGraphGenerated(UPCGComponent* Component);
	void HandlePlayerStartReadinessChanged(int32 Revision);
	void WakePlayerCountWaiters(int32 Revision);
	void WakeAllPlayerCountWaiters();
	void EvaluateCompletion();
	void CompleteGeneration(bool bSucceeded);
	void SetPhase(EP48PCGGenerationPhase Phase);
	bool AreServerGraphsComplete() const;
	int32 GetReadyControllerCount() const;
};
