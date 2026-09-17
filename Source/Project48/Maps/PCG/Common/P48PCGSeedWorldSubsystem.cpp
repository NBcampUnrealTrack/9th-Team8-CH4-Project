#include "P48PCGSeedWorldSubsystem.h"

#include "P48PCGNetworkSeedSettings.h"
#include "P48PCGSeedState.h"
#include "../../Objects/Spawn/P48PlayerStartRegistrySubsystem.h"
#include "../../../Character/P48PlayerController.h"
#include "../../../Game/P48GameModeBase.h"
#include "../../../GameplayMessageLibrary/Core/P48GameplayMessageLibrary.h"
#include "../../../GameplayMessageLibrary/Core/P48GameplayMessageTags.h"
#include "../../../GameplayMessageLibrary/Match/P48MatchMessagePayloads.h"
#include "../../../Character/P48PlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "TimerManager.h"

namespace P48PCGTimeouts
{
	constexpr float GraphGenerationSeconds = 30.0f;
	constexpr int32 PartialGraphRetryCount = 2;
	constexpr float ClientParticipationCutoffSeconds = 30.0f;
	constexpr float ClientEvictionSeconds = 60.0f;
}

void UP48PCGSeedWorldSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	PlayerCountChangedHandle = UP48GameplayMessageLibrary::Listen(
		this,
		P48GameplayTags::Match::PlayerCountChanged,
		&ThisClass::HandlePlayerCountChanged);
	if (!PlayerCountChangedHandle.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[P48PCG] Failed to subscribe to confirmed player counts in %s."), *World.GetName());
	}

	if (UP48PlayerStartRegistrySubsystem* Registry = World.GetSubsystem<UP48PlayerStartRegistrySubsystem>())
	{
		Registry->OnReadinessChanged.AddUObject(this, &ThisClass::HandlePlayerStartReadinessChanged);
	}
	DiscoverConsumers();
	// 로비처럼 NetworkSeed PCG 그래프가 없는 월드에서는 맵 생성 파이프라인을 시작하지 않는다.
	if (World.GetNetMode() != NM_Client && !Consumers.IsEmpty())
	{
		World.GetTimerManager().SetTimerForNextTick(this, &ThisClass::RequestGeneration);
	}
}

void UP48PCGSeedWorldSubsystem::Deinitialize()
{
	ClearGenerationTimers();
	UP48GameplayMessageLibrary::StopListening(PlayerCountChangedHandle);
	if (UP48PlayerStartRegistrySubsystem* Registry = GetWorld() ? GetWorld()->GetSubsystem<UP48PlayerStartRegistrySubsystem>() : nullptr)
	{
		Registry->OnReadinessChanged.RemoveAll(this);
	}
	for (const TWeakObjectPtr<UPCGComponent>& Consumer : Consumers)
	{
		if (UPCGComponent* Component = Consumer.Get())
		{
			Component->OnPCGGraphGeneratedDelegate.RemoveAll(this);
		}
	}
	Consumers.Reset();
	CompletedConsumers.Reset();
	ReadyControllers.Reset();
	PendingMapReadyControllers.Reset();
	PendingComponents.Reset();
	WakeAllPlayerCountWaiters();
	Super::Deinitialize();
}

FP48PCGGenerationContext UP48PCGSeedWorldSubsystem::GetGenerationContext() const
{
	FP48PCGGenerationContext Context;
	Context.Seed = Snapshot.Seed;
	Context.GenerationId = Snapshot.Revision;
	Context.RequiredPlayerCount = Snapshot.RequiredPlayerCount;
	return Context;
}

void UP48PCGSeedWorldSubsystem::SetReplicatedState(AP48PCGSeedState* InSeedState)
{
	if (!IsValid(InSeedState))
	{
		return;
	}
	SeedState = InSeedState;
	// A new replication actor must not erase a roster received before Seed creation.
	if (InSeedState->State.HasValidSeed()) { HandleReplicatedSnapshot(InSeedState->State); }
}

void UP48PCGSeedWorldSubsystem::HandleReplicatedSnapshot(const FP48PCGGenerationSnapshot& InSnapshot)
{
	if (!InSnapshot.HasValidSeed()) { return; }
	const bool bIsNewGeneration = InSnapshot.HasValidSeed() && InSnapshot.Revision != Snapshot.Revision;
	if (bIsNewGeneration)
	{
		WakePlayerCountWaiters(Snapshot.Revision);
	}
	Snapshot = InSnapshot;
	ConfirmedPlayerCount = FMath::Max(0, InSnapshot.RequiredPlayerCount);
	bPlayerCountLocked |= InSnapshot.HasConfirmedPlayerCount();
	if (InSnapshot.HasConfirmedPlayerCount())
	{
		WakePlayerCountWaiters(InSnapshot.Revision);
	}
	if (bIsNewGeneration)
	{
		BeginGeneration();
	}
}

void UP48PCGSeedWorldSubsystem::RequestGeneration()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	const int32 GeneratedSeed = UP48PCGNetworkSeedSettings::GenerateServerSeed();
	UE_LOG(LogTemp, Display, TEXT("[P48NetworkSeed] Server generated Seed=%d."), GeneratedSeed);
	RequestGenerationWithSeed(GeneratedSeed);
}

void UP48PCGSeedWorldSubsystem::RequestGenerationWithSeed(const int32 Seed)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	if (Seed == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[P48PCG] Rejected generation request with Seed=0."));
		return;
	}

	AP48PCGSeedState* StateActor = SeedState.Get();
	if (!StateActor)
	{
		for (TActorIterator<AP48PCGSeedState> It(World); It; ++It)
		{
			StateActor = *It;
			break;
		}
	}
	if (!StateActor)
	{
		StateActor = World->SpawnActor<AP48PCGSeedState>();
	}
	if (!StateActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[P48PCG] Failed to create the replicated seed state."));
		return;
	}

	SetReplicatedState(StateActor);
	if (Snapshot.HasValidSeed() && Snapshot.Seed == Seed && Snapshot.Phase != EP48PCGGenerationPhase::Failed)
	{
		return;
	}

	FP48PCGGenerationSnapshot NewSnapshot;
	NewSnapshot.Seed = Seed;
	NewSnapshot.Revision = Snapshot.Revision == MAX_int32 ? 1 : Snapshot.Revision + 1;
	NewSnapshot.RequiredPlayerCount = ConfirmedPlayerCount;
	NewSnapshot.Phase = EP48PCGGenerationPhase::Cleaning;
	StateActor->SetGenerationSnapshot(NewSnapshot);
	// Retry login decisions deferred until the Maps listener was registered.
	if (AP48GameModeBase* GameMode = World->GetAuthGameMode<AP48GameModeBase>())
	{
		GameMode->NotifyMapGenerationReadinessChanged();
	}
}

void UP48PCGSeedWorldSubsystem::SetRequiredPlayerCount(const int32 RequiredPlayerCount)
{
	if (bDebugPlayerCountOverride) { return; }
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || RequiredPlayerCount < 0)
	{
		return;
	}
	if (bPlayerCountLocked && RequiredPlayerCount > ConfirmedPlayerCount)
	{
		UE_LOG(LogTemp, Display,
			TEXT("[P48PlayerStartPolicy] Ignored late player-count increase: Locked=%d Requested=%d Revision=%d."),
			ConfirmedPlayerCount, RequiredPlayerCount, Snapshot.Revision);
		return;
	}

	ConfirmedPlayerCount = RequiredPlayerCount;
	bPlayerCountLocked |= RequiredPlayerCount > 0;
	if (!Snapshot.HasValidSeed())
	{
		return;
	}

	Snapshot.RequiredPlayerCount = RequiredPlayerCount;
	if (RequiredPlayerCount > 0 && Snapshot.Phase == EP48PCGGenerationPhase::WaitingForPlayerCount)
	{
		Snapshot.Phase = EP48PCGGenerationPhase::Generating;
	}
	else if (RequiredPlayerCount == 0 && AreServerGraphsComplete())
	{
		Snapshot.Phase = EP48PCGGenerationPhase::WaitingForPlayerCount;
	}

	if (AP48PCGSeedState* StateActor = SeedState.Get(); StateActor && StateActor->HasAuthority())
	{
		StateActor->SetGenerationSnapshot(Snapshot);
	}
	if (UP48PlayerStartRegistrySubsystem* Registry = World->GetSubsystem<UP48PlayerStartRegistrySubsystem>())
	{
		Registry->UpdateRequiredCount(Snapshot.Revision, RequiredPlayerCount);
	}
	if (RequiredPlayerCount > 0)
	{
		WakePlayerCountWaiters(Snapshot.Revision);
	}
	EvaluateCompletion();
}

void UP48PCGSeedWorldSubsystem::ApplyDebugPlayerCount(const int32 PlayerCount)
{
#if !UE_BUILD_SHIPPING
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || bDebugPlayerCountOverride || PlayerCount <= 0) { return; }
	// Debug bypasses only the roster lock/wait, never map generation or collision checks.
	bPlayerCountLocked = false;
	SetRequiredPlayerCount(PlayerCount);
	bDebugPlayerCountOverride = true;
	UE_LOG(LogTemp, Display, TEXT("[P48PlayerStartDebug] Revision=%d PlayerCount=%d. Broadcast counts are ignored for this debug session."), Snapshot.Revision, PlayerCount);
#endif
}

void UP48PCGSeedWorldSubsystem::RegisterPlayerCountWaiter(
	const int32 Revision,
	TWeakPtr<FPCGContextHandle> ContextHandle)
{
	UWorld* World = GetWorld();
	if (!World || Revision != Snapshot.Revision || Snapshot.HasConfirmedPlayerCount())
	{
		FPCGContext::FSharedContext<FPCGContext> SharedContext(ContextHandle);
		if (FPCGContext* Context = SharedContext.Get())
		{
			Context->bIsPaused = false;
		}
		return;
	}

	PlayerCountWaiters.Emplace(Revision, MoveTemp(ContextHandle));
	if (World->GetNetMode() != NM_Client)
	{
		SetPhase(EP48PCGGenerationPhase::WaitingForPlayerCount);
	}
}

void UP48PCGSeedWorldSubsystem::HandlePlayerCountChanged(
	FGameplayTag,
	const FP48MatchPlayerCountMessage& Message)
{
	SetRequiredPlayerCount(FMath::Max(0, Message.PlayerCount));
}

void UP48PCGSeedWorldSubsystem::BeginGeneration()
{
	ClearGenerationTimers();
	bGenerationRunning = true;
	bClientReadinessCutoffPassed = false;
	GraphRetryAttempt = 0;
	CompletedConsumers.Reset();
	ReadyControllers.Reset();
	PendingMapReadyControllers.Reset();
	PendingComponents.Reset();
	DiscoverConsumers();
	ApplySeedToConsumers();

	if (UWorld* World = GetWorld(); World && World->GetNetMode() != NM_Client)
	{
		if (UP48PlayerStartRegistrySubsystem* Registry = World->GetSubsystem<UP48PlayerStartRegistrySubsystem>())
		{
			Registry->BeginGeneration(Snapshot.Revision, Snapshot.RequiredPlayerCount);
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::CleanupGraphs);
	}
}

bool UP48PCGSeedWorldSubsystem::GraphUsesNetworkSeed(const UPCGGraph* Graph)
{
	return Graph && !Graph->FindNodesWithSettings(UP48PCGNetworkSeedSettings::StaticClass(), true).IsEmpty();
}

bool UP48PCGSeedWorldSubsystem::HasNetworkSeedConsumer(UWorld* World)
{
	if (!World)
	{
		return false;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UPCGComponent*> Components;
		It->GetComponents(Components);
		for (const UPCGComponent* Component : Components)
		{
			if (Component && GraphUsesNetworkSeed(Component->GetGraph()))
			{
				return true;
			}
		}
	}
	return false;
}

void UP48PCGSeedWorldSubsystem::DiscoverConsumers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (auto It = Consumers.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UPCGComponent*> Components;
		It->GetComponents(Components);
		for (UPCGComponent* Component : Components)
		{
			if (Component && GraphUsesNetworkSeed(Component->GetGraph()))
			{
				RegisterConsumer(Component);
			}
		}
	}
}

void UP48PCGSeedWorldSubsystem::RegisterConsumer(UPCGComponent* Component)
{
	if (!IsValid(Component))
	{
		return;
	}
	if (bGenerationRunning && Snapshot.HasValidSeed())
	{
		Component->Seed = Snapshot.Seed;
	}
	if (Consumers.Contains(Component))
	{
		return;
	}
	Consumers.Add(Component);
	Component->OnPCGGraphGeneratedDelegate.AddUObject(this, &ThisClass::HandleGraphGenerated);
}

void UP48PCGSeedWorldSubsystem::ApplySeedToConsumers()
{
	for (const TWeakObjectPtr<UPCGComponent>& WeakComponent : Consumers)
	{
		if (UPCGComponent* Component = WeakComponent.Get())
		{
			Component->Seed = Snapshot.Seed;
		}
	}
}

void UP48PCGSeedWorldSubsystem::CleanupGraphs()
{
	PendingComponents.Reset();
	for (const TWeakObjectPtr<UPCGComponent>& WeakComponent : Consumers)
	{
		if (UPCGComponent* Component = WeakComponent.Get())
		{
			Component->CleanupLocalImmediate(true, true);
			PendingComponents.Add(Component);
		}
	}
	if (PendingComponents.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[P48PCG] Generation %d has no NetworkSeed consumer."), Snapshot.Revision);
		CompleteGeneration(false);
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::GenerateGraphs);
	}
}

void UP48PCGSeedWorldSubsystem::GenerateGraphs()
{
	SetPhase(EP48PCGGenerationPhase::Generating);
	for (const TWeakObjectPtr<UPCGComponent>& WeakComponent : PendingComponents)
	{
		if (UPCGComponent* Component = WeakComponent.Get())
		{
			Component->GenerateLocal(true);
		}
	}
	PendingComponents.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(GraphGenerationWatchdogTimer, this,
			&ThisClass::HandleGraphGenerationTimeout,
			P48PCGTimeouts::GraphGenerationSeconds, false);
	}
}

void UP48PCGSeedWorldSubsystem::HandleGraphGenerationTimeout()
{
	if (!bGenerationRunning || AreServerGraphsComplete()) return;

	++GraphRetryAttempt;
	for (const TWeakObjectPtr<UPCGComponent>& Consumer : Consumers)
	{
		UPCGComponent* Component = Consumer.Get();
		if (!Component || CompletedConsumers.Contains(Consumer)) continue;
		UE_LOG(LogTemp, Error,
			TEXT("[P48PCG] Graph timeout. Revision=%d Attempt=%d Component=%s IsGenerating=%d"),
			Snapshot.Revision, GraphRetryAttempt, *Component->GetPathName(),
			Component->IsGenerating());
	}

	UWorld* World = GetWorld();
	if (!World) return;
	if (GraphRetryAttempt > P48PCGTimeouts::PartialGraphRetryCount
		&& World->GetNetMode() != NM_Client)
	{
		RestartServerGeneration();
		return;
	}
	if (GraphRetryAttempt > P48PCGTimeouts::PartialGraphRetryCount)
	{
		// 서버가 60초 유예 종료를 결정할 때까지 해당 클라이언트는 로컬 생성을 계속 시도한다.
		GraphRetryAttempt = 1;
	}

	for (const TWeakObjectPtr<UPCGComponent>& Consumer : Consumers)
	{
		if (UPCGComponent* Component = Consumer.Get();
			Component && !CompletedConsumers.Contains(Consumer))
		{
			Component->CancelGeneration();
			Component->CleanupLocalImmediate(true, true);
			Component->Seed = Snapshot.Seed;
		}
	}
	World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RetryIncompleteGraphs);
}

void UP48PCGSeedWorldSubsystem::RetryIncompleteGraphs()
{
	if (!bGenerationRunning) return;
	for (const TWeakObjectPtr<UPCGComponent>& Consumer : Consumers)
	{
		if (UPCGComponent* Component = Consumer.Get();
			Component && !CompletedConsumers.Contains(Consumer))
		{
			Component->GenerateLocal(true);
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(GraphGenerationWatchdogTimer, this,
			&ThisClass::HandleGraphGenerationTimeout,
			P48PCGTimeouts::GraphGenerationSeconds, false);
	}
}

void UP48PCGSeedWorldSubsystem::RestartServerGeneration()
{
	UWorld* World = GetWorld();
	AP48PCGSeedState* StateActor = SeedState.Get();
	if (!World || World->GetNetMode() == NM_Client || !StateActor || !StateActor->HasAuthority())
	{
		return;
	}
	for (const TWeakObjectPtr<UPCGComponent>& Consumer : Consumers)
	{
		if (UPCGComponent* Component = Consumer.Get()) Component->CancelGeneration();
	}

	FP48PCGGenerationSnapshot RetrySnapshot = Snapshot;
	RetrySnapshot.Revision = Snapshot.Revision == MAX_int32 ? 1 : Snapshot.Revision + 1;
	RetrySnapshot.Phase = EP48PCGGenerationPhase::Cleaning;
	UE_LOG(LogTemp, Error,
		TEXT("[P48PCG] Restarting the complete server generation. Seed=%d Revision=%d->%d"),
		Snapshot.Seed, Snapshot.Revision, RetrySnapshot.Revision);
	StateActor->SetGenerationSnapshot(RetrySnapshot);
}

void UP48PCGSeedWorldSubsystem::PrepareGeneratedSurfacesForNetworking(UPCGComponent* Component) const
{
	AActor* Owner = IsValid(Component) ? Component->GetOwner() : nullptr;
	if (!IsValid(Owner))
	{
		return;
	}

	TInlineComponentArray<UInstancedStaticMeshComponent*> SurfaceComponents;
	Owner->GetComponents(SurfaceComponents);

	int32 PreparedCount = 0;
	for (UInstancedStaticMeshComponent* SurfaceComponent : SurfaceComponents)
	{
		if (!IsValid(SurfaceComponent)
			|| SurfaceComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision
			|| SurfaceComponent->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
		{
			continue;
		}

		// PCG creates these components at runtime. The shared graph and seed give them
		// matching paths on server and clients, so register those paths before a
		// Character can replicate the component as its movement base.
		SurfaceComponent->SetNetAddressable();
		++PreparedCount;

		if (SurfaceComponent->Mobility != EComponentMobility::Static)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[P48PCG] Generated walkable surface is not Static: %s"),
				*SurfaceComponent->GetPathName());
		}
	}

	UE_LOG(LogTemp, Display,
		TEXT("[P48PCG] Prepared %d generated walkable surface component(s) for networking. Owner=%s Revision=%d"),
		PreparedCount, *Owner->GetPathName(), Snapshot.Revision);
}

void UP48PCGSeedWorldSubsystem::HandleGraphGenerated(UPCGComponent* Component)
{
	if (!bGenerationRunning || !IsValid(Component) || !Consumers.Contains(Component))
	{
		return;
	}
	PrepareGeneratedSurfacesForNetworking(Component);
	CompletedConsumers.Add(Component);
	if (!AreServerGraphsComplete())
	{
		return;
	}
	if (UWorld* CurrentWorld = GetWorld())
	{
		CurrentWorld->GetTimerManager().ClearTimer(GraphGenerationWatchdogTimer);
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (World->GetNetMode() == NM_Client)
	{
		World->GetTimerManager().SetTimer(ClientReportTimer, this, &ThisClass::TryReportClientCompletion, 0.5f, true);
		TryReportClientCompletion();
		return;
	}

	SetPhase(Snapshot.HasConfirmedPlayerCount()
		? EP48PCGGenerationPhase::WaitingForPlayerStarts
		: EP48PCGGenerationPhase::WaitingForPlayerCount);
	if (UP48PlayerStartRegistrySubsystem* Registry = World->GetSubsystem<UP48PlayerStartRegistrySubsystem>())
	{
		Registry->SealRegistration(Snapshot.Revision);
	}
	EvaluateCompletion();
}

void UP48PCGSeedWorldSubsystem::TryReportClientCompletion()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client) { return; }
	if (Snapshot.Phase == EP48PCGGenerationPhase::Failed)
	{
		World->GetTimerManager().ClearTimer(ClientReportTimer);
		return;
	}
	if (!AreServerGraphsComplete()) { return; }
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AP48PlayerController* Controller = Cast<AP48PlayerController>(It->Get()); Controller && Controller->IsLocalController())
		{
			Controller->ReportMapGenerationComplete(Snapshot.Revision);
			World->GetTimerManager().ClearTimer(ClientReportTimer);
			bGenerationRunning = false;
			break;
		}
	}
}

void UP48PCGSeedWorldSubsystem::NotifyClientGenerationComplete(APlayerController* PlayerController, const int32 Revision)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !IsValid(PlayerController) || Revision != Snapshot.Revision)
	{
		return;
	}
	// Keep early reports. GetReadyControllerCount only counts confirmed participants.
	if (PlayerController->GetWorld() != GetWorld()) { return; }
	ReadyControllers.FindOrAdd(PlayerController) = Revision;
	if (PendingMapReadyControllers.Remove(PlayerController) > 0)
	{
		if (AP48GameModeBase* GameMode = GetWorld()->GetAuthGameMode<AP48GameModeBase>())
		{
			GameMode->HandleLatePCGClientReady(PlayerController);
		}
	}
	EvaluateCompletion();
	if (bClientReadinessCutoffPassed
		&& Snapshot.Phase == EP48PCGGenerationPhase::WaitingForClients)
	{
		HandleClientReadinessCutoff();
	}
}

void UP48PCGSeedWorldSubsystem::NotifyControllerJoined(APlayerController* PlayerController)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !IsValid(PlayerController))
	{
		return;
	}
	EvaluateCompletion();
}

bool UP48PCGSeedWorldSubsystem::IsReadyForController(const APlayerController* PlayerController) const
{
	if (!IsValid(PlayerController) || !Snapshot.IsReady())
	{
		return false;
	}
	const AP48PlayerState* PlayerState = PlayerController->GetPlayerState<AP48PlayerState>();
	if (!IsValid(PlayerState) || !PlayerState->IsMatchParticipant())
	{
		return false;
	}
	if (PlayerController->IsLocalController() && GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		return true;
	}
	return ReadyControllers.FindRef(PlayerController) == Snapshot.Revision;
}

void UP48PCGSeedWorldSubsystem::HandlePlayerStartReadinessChanged(const int32 Revision)
{
	if (Revision == Snapshot.Revision)
	{
		EvaluateCompletion();
	}
}

void UP48PCGSeedWorldSubsystem::WakePlayerCountWaiters(const int32 Revision)
{
	for (int32 Index = PlayerCountWaiters.Num() - 1; Index >= 0; --Index)
	{
		if (PlayerCountWaiters[Index].Key != Revision)
		{
			continue;
		}

		FPCGContext::FSharedContext<FPCGContext> SharedContext(PlayerCountWaiters[Index].Value);
		if (FPCGContext* Context = SharedContext.Get())
		{
			Context->bIsPaused = false;
		}
		PlayerCountWaiters.RemoveAtSwap(Index);
	}
}

void UP48PCGSeedWorldSubsystem::WakeAllPlayerCountWaiters()
{
	for (const TPair<int32, TWeakPtr<FPCGContextHandle>>& Waiter : PlayerCountWaiters)
	{
		FPCGContext::FSharedContext<FPCGContext> SharedContext(Waiter.Value);
		if (FPCGContext* Context = SharedContext.Get())
		{
			Context->bIsPaused = false;
		}
	}
	PlayerCountWaiters.Reset();
}

void UP48PCGSeedWorldSubsystem::EvaluateCompletion()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || Snapshot.Phase == EP48PCGGenerationPhase::Failed || !AreServerGraphsComplete())
	{
		return;
	}
	if (!Snapshot.HasConfirmedPlayerCount())
	{
		SetPhase(EP48PCGGenerationPhase::WaitingForPlayerCount);
		return;
	}
	const UP48PlayerStartRegistrySubsystem* Registry = World->GetSubsystem<UP48PlayerStartRegistrySubsystem>();
	if (!Registry)
	{
		CompleteGeneration(false);
		return;
	}
	const EP48PlayerStartLayoutState LayoutState = Registry->GetState(Snapshot.Revision);
	if (LayoutState == EP48PlayerStartLayoutState::Failed)
	{
		CompleteGeneration(false);
		return;
	}
	if (LayoutState != EP48PlayerStartLayoutState::Ready)
	{
		SetPhase(EP48PCGGenerationPhase::WaitingForPlayerStarts);
		return;
	}
	if (GetReadyControllerCount() < Snapshot.RequiredPlayerCount)
	{
		SetPhase(EP48PCGGenerationPhase::WaitingForClients);
		StartClientReadinessTimers();
		return;
	}
	CompleteGeneration(true);
}

void UP48PCGSeedWorldSubsystem::StartClientReadinessTimers()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client
		|| bClientReadinessCutoffPassed
		|| World->GetTimerManager().IsTimerActive(ClientReadinessCutoffTimer))
	{
		return;
	}
	World->GetTimerManager().SetTimer(ClientReadinessCutoffTimer, this,
		&ThisClass::HandleClientReadinessCutoff,
		P48PCGTimeouts::ClientParticipationCutoffSeconds, false);
	World->GetTimerManager().SetTimer(ClientReadinessEvictionTimer, this,
		&ThisClass::HandleClientReadinessEviction,
		P48PCGTimeouts::ClientEvictionSeconds, false);
	UE_LOG(LogTemp, Display,
		TEXT("[P48PCG] Waiting for clients. Revision=%d Cutoff=%.0fs Eviction=%.0fs"),
		Snapshot.Revision, P48PCGTimeouts::ClientParticipationCutoffSeconds,
		P48PCGTimeouts::ClientEvictionSeconds);
}

void UP48PCGSeedWorldSubsystem::HandleClientReadinessCutoff()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	bClientReadinessCutoffPassed = true;

	AP48GameModeBase* GameMode = World->GetAuthGameMode<AP48GameModeBase>();
	if (!GameMode) return;
	TArray<APlayerController*> ExcludedControllers;
	if (GameMode->ApplyPCGReadyParticipantRoster(
		GetReadyParticipantControllers(), ExcludedControllers, false))
	{
		for (APlayerController* Controller : ExcludedControllers)
		{
			PendingMapReadyControllers.Add(Controller);
		}
		EvaluateCompletion();
	}
}

void UP48PCGSeedWorldSubsystem::HandleClientReadinessEviction()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) return;
	AP48GameModeBase* GameMode = World->GetAuthGameMode<AP48GameModeBase>();
	if (!GameMode) return;

	TArray<APlayerController*> ExcludedControllers;
	if (!Snapshot.IsReady())
	{
		GameMode->ApplyPCGReadyParticipantRoster(
			GetReadyParticipantControllers(), ExcludedControllers, true);
		for (APlayerController* Controller : ExcludedControllers)
		{
			PendingMapReadyControllers.Add(Controller);
		}
		EvaluateCompletion();
	}

	TArray<TWeakObjectPtr<APlayerController>> ControllersToExpel =
		PendingMapReadyControllers.Array();
	PendingMapReadyControllers.Reset();
	for (const TWeakObjectPtr<APlayerController>& WeakController : ControllersToExpel)
	{
		if (APlayerController* Controller = WeakController.Get())
		{
			GameMode->ExpelPCGUnreadyPlayer(Controller);
		}
	}
}

TArray<APlayerController*> UP48PCGSeedWorldSubsystem::GetReadyParticipantControllers() const
{
	TArray<APlayerController*> Result;
	UWorld* World = GetWorld();
	if (!World) return Result;
	const AP48GameModeBase* GameMode = World->GetAuthGameMode<AP48GameModeBase>();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		const AP48PlayerState* PlayerState = IsValid(Controller)
			? Controller->GetPlayerState<AP48PlayerState>() : nullptr;
		if (GameMode && GameMode->IsPCGRequiredParticipant(PlayerState)
			&& ((Controller->IsLocalController() && World->GetNetMode() != NM_DedicatedServer)
				|| ReadyControllers.FindRef(Controller) == Snapshot.Revision))
		{
			Result.Add(Controller);
		}
	}
	return Result;
}

void UP48PCGSeedWorldSubsystem::ClearGenerationTimers(const bool bClearEvictionTimer)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ClientReportTimer);
		World->GetTimerManager().ClearTimer(GraphGenerationWatchdogTimer);
		World->GetTimerManager().ClearTimer(ClientReadinessCutoffTimer);
		if (bClearEvictionTimer)
		{
			World->GetTimerManager().ClearTimer(ClientReadinessEvictionTimer);
		}
	}
}

bool UP48PCGSeedWorldSubsystem::AreServerGraphsComplete() const
{
	if (Consumers.IsEmpty())
	{
		return false;
	}
	for (const TWeakObjectPtr<UPCGComponent>& Consumer : Consumers)
	{
		if (!Consumer.IsValid() || !CompletedConsumers.Contains(Consumer))
		{
			return false;
		}
	}
	return true;
}

int32 UP48PCGSeedWorldSubsystem::GetReadyControllerCount() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}
	const AP48GameModeBase* GameMode = World->GetAuthGameMode<AP48GameModeBase>();
	int32 Count = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (!Controller)
		{
			continue;
		}

		const AP48PlayerState* PlayerState = Controller->GetPlayerState<AP48PlayerState>();
		if (!GameMode || !GameMode->IsPCGRequiredParticipant(PlayerState))
		{
			continue;
		}

		if ((Controller->IsLocalController() && World->GetNetMode() != NM_DedicatedServer) || ReadyControllers.FindRef(Controller) == Snapshot.Revision)
		{
			++Count;
		}
	}
	return Count;
}

void UP48PCGSeedWorldSubsystem::CompleteGeneration(const bool bSucceeded)
{
	if (Snapshot.Revision <= 0)
	{
		return;
	}
	const bool bAlreadyReported = LastCompletedRevision == Snapshot.Revision;
	const EP48PCGGenerationPhase NewPhase = bSucceeded ? EP48PCGGenerationPhase::Ready : EP48PCGGenerationPhase::Failed;
	const bool bPhaseChanged = Snapshot.Phase != NewPhase;
	bGenerationRunning = false;
	LastCompletedRevision = Snapshot.Revision;
	ClearGenerationTimers(bSucceeded && bClientReadinessCutoffPassed ? false : true);
	SetPhase(NewPhase);
	if (!bAlreadyReported)
	{
		UE_LOG(LogTemp, Display, TEXT("[P48PCG] Generation=%d Seed=%d Players=%d Succeeded=%d"), Snapshot.Revision, Snapshot.Seed, Snapshot.RequiredPlayerCount, bSucceeded);
	}
	if (bAlreadyReported && !bPhaseChanged) { return; }
	if (AP48GameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AP48GameModeBase>() : nullptr)
	{
		GameMode->NotifyMapGenerationReadinessChanged();
	}
}

void UP48PCGSeedWorldSubsystem::SetPhase(const EP48PCGGenerationPhase Phase)
{
	if (Snapshot.Phase == Phase)
	{
		return;
	}
	Snapshot.Phase = Phase;
	if (AP48PCGSeedState* StateActor = SeedState.Get(); StateActor && StateActor->HasAuthority())
	{
		StateActor->SetGenerationSnapshot(Snapshot);
	}
}
