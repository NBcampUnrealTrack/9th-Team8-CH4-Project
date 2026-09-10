#include "P48PCGSeedWorldSubsystem.h"

#include "P48PCGNetworkSeedSettings.h"
#include "P48PCGSeedState.h"
#include "../../Objects/Spawn/P48PlayerStartRegistrySubsystem.h"
#include "../../../Character/P48PlayerController.h"
#include "../../../Game/P48GameModeBase.h"
#include "../../../GameplayMessageLibrary/Core/P48GameplayMessageLibrary.h"
#include "../../../GameplayMessageLibrary/Core/P48GameplayMessageTags.h"
#include "../../../GameplayMessageLibrary/Map/P48MapMessagePayloads.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "TimerManager.h"

void UP48PCGSeedWorldSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	GenerationRequestHandle = UP48GameplayMessageLibrary::Listen(
		this,
		P48GameplayTags::Map::GenerationRequested,
		&ThisClass::HandleGenerationRequested);

	if (UP48PlayerStartRegistrySubsystem* Registry = World.GetSubsystem<UP48PlayerStartRegistrySubsystem>())
	{
		Registry->OnReadinessChanged.AddUObject(this, &ThisClass::HandlePlayerStartReadinessChanged);
	}
	DiscoverConsumers();
}

void UP48PCGSeedWorldSubsystem::Deinitialize()
{
	UP48GameplayMessageLibrary::StopListening(GenerationRequestHandle);
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
	PendingComponents.Reset();
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
	HandleReplicatedSnapshot(InSeedState->State);
}

void UP48PCGSeedWorldSubsystem::HandleReplicatedSnapshot(const FP48PCGGenerationSnapshot& InSnapshot)
{
	const bool bIsNewGeneration = InSnapshot.HasValidRequest() && InSnapshot.Revision != Snapshot.Revision;
	Snapshot = InSnapshot;
	if (bIsNewGeneration)
	{
		BeginGeneration();
	}
}

void UP48PCGSeedWorldSubsystem::RequestGeneration(const int32 RequiredPlayerCount, const int32 Seed)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	if (RequiredPlayerCount <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[P48PCG] Rejected generation request with PlayerCount=%d."), RequiredPlayerCount);
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
	if (Snapshot.HasValidRequest() && Snapshot.Seed == Seed && Snapshot.RequiredPlayerCount == RequiredPlayerCount && Snapshot.Phase != EP48PCGGenerationPhase::Failed)
	{
		return;
	}

	FP48PCGGenerationSnapshot NewSnapshot;
	NewSnapshot.Seed = Seed;
	NewSnapshot.Revision = Snapshot.Revision == MAX_int32 ? 1 : Snapshot.Revision + 1;
	NewSnapshot.RequiredPlayerCount = RequiredPlayerCount;
	NewSnapshot.Phase = EP48PCGGenerationPhase::Cleaning;
	StateActor->SetGenerationSnapshot(NewSnapshot);
}

void UP48PCGSeedWorldSubsystem::HandleGenerationRequested(FGameplayTag, const FP48MapGenerationRequestMessage& Message)
{
	RequestGeneration(Message.PlayerCount, Message.Seed);
}

void UP48PCGSeedWorldSubsystem::BeginGeneration()
{
	bGenerationRunning = true;
	CompletedConsumers.Reset();
	ReadyControllers.Reset();
	PendingComponents.Reset();
	DiscoverConsumers();

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

void UP48PCGSeedWorldSubsystem::DiscoverConsumers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UPCGComponent*> Components;
		It->GetComponents(Components);
		for (UPCGComponent* Component : Components)
		{
			const UPCGGraph* Graph = Component ? Component->GetGraph() : nullptr;
			if (!Graph)
			{
				continue;
			}
			for (const UPCGNode* Node : Graph->GetNodes())
			{
				if (Node && Cast<UP48PCGNetworkSeedSettings>(Node->GetSettings()))
				{
					RegisterConsumer(Component);
					break;
				}
			}
		}
	}
}

void UP48PCGSeedWorldSubsystem::RegisterConsumer(UPCGComponent* Component)
{
	if (!IsValid(Component) || Consumers.Contains(Component))
	{
		return;
	}
	Consumers.Add(Component);
	Component->OnPCGGraphGeneratedDelegate.AddUObject(this, &ThisClass::HandleGraphGenerated);
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
}

void UP48PCGSeedWorldSubsystem::HandleGraphGenerated(UPCGComponent* Component)
{
	if (!bGenerationRunning || !IsValid(Component) || !Consumers.Contains(Component))
	{
		return;
	}
	CompletedConsumers.Add(Component);
	if (!AreServerGraphsComplete())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (World->GetNetMode() == NM_Client)
	{
		if (LastClientReportedRevision == Snapshot.Revision)
		{
			return;
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (AP48PlayerController* Controller = Cast<AP48PlayerController>(It->Get()); Controller && Controller->IsLocalController())
			{
				LastClientReportedRevision = Snapshot.Revision;
				Controller->ReportMapGenerationComplete(Snapshot.Revision);
				break;
			}
		}
		return;
	}

	SetPhase(EP48PCGGenerationPhase::WaitingForPlayerStarts);
	if (UP48PlayerStartRegistrySubsystem* Registry = World->GetSubsystem<UP48PlayerStartRegistrySubsystem>())
	{
		Registry->SealRegistration(Snapshot.Revision);
	}
	EvaluateCompletion();
}

void UP48PCGSeedWorldSubsystem::NotifyClientGenerationComplete(APlayerController* PlayerController, const int32 Revision)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !IsValid(PlayerController) || Revision != Snapshot.Revision)
	{
		return;
	}
	ReadyControllers.FindOrAdd(PlayerController) = Revision;
	EvaluateCompletion();
}

void UP48PCGSeedWorldSubsystem::NotifyControllerJoined(APlayerController* PlayerController)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !IsValid(PlayerController))
	{
		return;
	}
	if (!PlayerController->IsLocalController() || GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		ReadyControllers.Remove(PlayerController);
	}
	EvaluateCompletion();
}

bool UP48PCGSeedWorldSubsystem::IsReadyForController(const APlayerController* PlayerController) const
{
	if (!IsValid(PlayerController) || !Snapshot.IsReady())
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

void UP48PCGSeedWorldSubsystem::EvaluateCompletion()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !AreServerGraphsComplete())
	{
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
		return;
	}
	CompleteGeneration(true);
}

bool UP48PCGSeedWorldSubsystem::AreServerGraphsComplete() const
{
	if (Consumers.IsEmpty())
	{
		return false;
	}
	for (const TWeakObjectPtr<UPCGComponent>& Consumer : Consumers)
	{
		if (Consumer.IsValid() && !CompletedConsumers.Contains(Consumer))
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
	int32 Count = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (!Controller)
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
	if (LastCompletedRevision == Snapshot.Revision || Snapshot.Revision <= 0)
	{
		return;
	}
	bGenerationRunning = false;
	LastCompletedRevision = Snapshot.Revision;
	SetPhase(bSucceeded ? EP48PCGGenerationPhase::Ready : EP48PCGGenerationPhase::Failed);
	UP48GameplayMessageLibrary::Broadcast(
		this,
		P48GameplayTags::Map::GenerationCompleted,
		FP48MapGenerationCompletedMessage(Snapshot.Revision, Snapshot.Seed, bSucceeded));

	UE_LOG(LogTemp, Display, TEXT("[P48PCG] Generation=%d Seed=%d Players=%d Succeeded=%d"), Snapshot.Revision, Snapshot.Seed, Snapshot.RequiredPlayerCount, bSucceeded);
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
