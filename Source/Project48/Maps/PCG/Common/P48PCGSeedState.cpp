#include "P48PCGSeedState.h"
#include "PCGComponent.h"
#include "Net/UnrealNetwork.h"
#include "Misc/Guid.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Project48/Character/P48PlayerController.h"
#include "Project48/Game/P48GameModeBase.h"
#include "../../Objects/Spawn/P48PlayerStart.h"

AP48PCGSeedState::AP48PCGSeedState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bNetLoadOnClient = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
}

void AP48PCGSeedState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && State.Revision == 0) { SetMapSeed(static_cast<int32>(GetTypeHash(FGuid::NewGuid()) & MAX_int32)); }
}

void AP48PCGSeedState::SetMapSeed(int32 Seed)
{
	if (!HasAuthority()) { return; }
	State.Seed = Seed;
	State.Revision = State.Revision == MAX_int32 ? 1 : State.Revision + 1;
	State.bMapReady = false;
	bServerGenerationComplete = false;
	bPlayerStartLayoutRequired = true;
	bPlayerStartLayoutValid = false;
	bPlayerStartTimeoutLogged = false;
	RequestedPlayerStartCount = 0;
	SelectedPlayerStartCount = 0;
	PlayerStartRegistrationDeadline = 0.0;
	RegisteredPlayerStarts.Reset();
	ReadyControllers.Reset();
	ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("[P48NetworkSeed] Server Seed=%d Revision=%d"), State.Seed, State.Revision);
}

void AP48PCGSeedState::ReportPlayerStartLayout(const int32 RequestedCount, const int32 SelectedCount)
{
	if (!HasAuthority() || State.Revision == 0)
	{
		return;
	}
	bPlayerStartLayoutRequired = true;
	RequestedPlayerStartCount = FMath::Max(0, RequestedCount);
	SelectedPlayerStartCount = FMath::Max(0, SelectedCount);
	bPlayerStartLayoutValid = false;
	bPlayerStartTimeoutLogged = false;
	PlayerStartRegistrationDeadline = 0.0;
	RefreshPlayerStartReadiness();
	UE_LOG(LogTemp, Display, TEXT("[P48PlayerStartLayout] Revision=%d Requested=%d Selected=%d Registered=%d"), State.Revision, RequestedPlayerStartCount, SelectedPlayerStartCount, GetRegisteredPlayerStartCount());
}

void AP48PCGSeedState::RegisterGeneratedPlayerStart(AP48PlayerStart* PlayerStart)
{
	if (!HasAuthority() || !IsValid(PlayerStart) || State.Revision <= 0)
	{
		return;
	}

	TSet<int32> UsedSlots;
	for (const TWeakObjectPtr<AP48PlayerStart>& ExistingPlayerStart : RegisteredPlayerStarts)
	{
		if (const AP48PlayerStart* Existing = ExistingPlayerStart.Get(); Existing && Existing != PlayerStart && Existing->GenerationId == State.Revision && Existing->SpawnSlotIndex != INDEX_NONE)
		{
			UsedSlots.Add(Existing->SpawnSlotIndex);
		}
	}

	if (PlayerStart->SpawnSlotIndex == INDEX_NONE || UsedSlots.Contains(PlayerStart->SpawnSlotIndex))
	{
		int32 AvailableSlot = 0;
		while (UsedSlots.Contains(AvailableSlot))
		{
			++AvailableSlot;
		}
		PlayerStart->SpawnSlotIndex = AvailableSlot;
	}

	PlayerStart->GenerationId = State.Revision;
	RegisteredPlayerStarts.Add(PlayerStart);
	RefreshPlayerStartReadiness();
	UE_LOG(LogTemp, Display, TEXT("[P48PlayerStartRegister] Revision=%d Name=%s Slot=%d Island=%d Registered=%d/%d"), State.Revision, *GetNameSafe(PlayerStart), PlayerStart->SpawnSlotIndex, PlayerStart->IslandIndex, GetRegisteredPlayerStartCount(), RequestedPlayerStartCount);
}

void AP48PCGSeedState::UnregisterGeneratedPlayerStart(AP48PlayerStart* PlayerStart)
{
	if (!HasAuthority() || !PlayerStart)
	{
		return;
	}

	RegisteredPlayerStarts.Remove(PlayerStart);
	RefreshPlayerStartReadiness();
}

void AP48PCGSeedState::RegisterConsumer(UPCGComponent* Component)
{
	if (!Component) { return; }
	Consumers.FindOrAdd(Component) = State.Revision;
	if (!BoundConsumers.Contains(Component))
	{
		BoundConsumers.Add(Component);
		Component->OnPCGGraphGeneratedDelegate.AddUObject(this, &ThisClass::HandleGraphGenerated);
	}
}

void AP48PCGSeedState::HandleGraphGenerated(UPCGComponent* Component)
{
	if (!Component || State.Revision == 0) { return; }
	const int32* ConsumerRevision = Consumers.Find(Component);
	if (!ConsumerRevision || *ConsumerRevision != State.Revision) { return; }
	CompletedConsumers.FindOrAdd(Component) = State.Revision;
	if (!IsLocalGenerationComplete()) { return; }
	if (HasAuthority())
	{
		bServerGenerationComplete = true;
		PlayerStartRegistrationDeadline = GetWorld()->GetTimeSeconds() + 1.0;
		RefreshPlayerStartReadiness();
		return;
	}
	if (LastReportedRevision == State.Revision) { return; }
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AP48PlayerController* PlayerController = Cast<AP48PlayerController>(It->Get()); PlayerController && PlayerController->IsLocalController())
		{
			LastReportedRevision = State.Revision;
			PlayerController->ReportMapGenerationComplete(State.Revision);
			break;
		}
	}
}

int32 AP48PCGSeedState::GetRegisteredPlayerStartCount() const
{
	TSet<int32> SpawnSlots;
	for (const TWeakObjectPtr<AP48PlayerStart>& RegisteredPlayerStart : RegisteredPlayerStarts)
	{
		const AP48PlayerStart* PlayerStart = RegisteredPlayerStart.Get();
		if (PlayerStart && PlayerStart->GenerationId == State.Revision && PlayerStart->SpawnSlotIndex != INDEX_NONE)
		{
			SpawnSlots.Add(PlayerStart->SpawnSlotIndex);
		}
	}
	return SpawnSlots.Num();
}

void AP48PCGSeedState::RefreshPlayerStartReadiness()

{
	if (!HasAuthority())
	{
		return;
	}

	const int32 RegisteredCount = GetRegisteredPlayerStartCount();
	const bool bWasValid = bPlayerStartLayoutValid;
	bPlayerStartLayoutValid = bPlayerStartLayoutRequired &&
		RequestedPlayerStartCount > 0 &&
		SelectedPlayerStartCount >= RequestedPlayerStartCount &&
		RegisteredCount >= RequestedPlayerStartCount;

	if (!bWasValid && bPlayerStartLayoutValid)
	{
		UE_LOG(LogTemp, Display, TEXT("[P48PlayerStartReady] Revision=%d Registered=%d/%d"), State.Revision, RegisteredCount, RequestedPlayerStartCount);
	}

	UpdateMapReady();
}

bool AP48PCGSeedState::IsLocalGenerationComplete() const
{
	if (Consumers.IsEmpty()) { return false; }
	for (const TPair<TWeakObjectPtr<UPCGComponent>, int32>& Consumer : Consumers)
	{
		if (Consumer.Key.IsValid() && CompletedConsumers.FindRef(Consumer.Key) != State.Revision) { return false; }
	}
	return true;
}

void AP48PCGSeedState::NotifyClientGenerationComplete(APlayerController* PlayerController, const int32 Revision)
{
	if (!HasAuthority() || !PlayerController || Revision != State.Revision) { return; }
	ReadyControllers.FindOrAdd(PlayerController) = Revision;
	UE_LOG(LogTemp, Display, TEXT("[P48MapReady] Client=%s Revision=%d"), *GetNameSafe(PlayerController), Revision);
	UpdateMapReady();
}

void AP48PCGSeedState::NotifyControllerJoined(APlayerController* PlayerController)
{
	if (!HasAuthority() || !PlayerController) { return; }
	if (!PlayerController->IsLocalController() || GetNetMode() == NM_DedicatedServer)
	{
		ReadyControllers.Remove(PlayerController);
	}
	UpdateMapReady();
}

bool AP48PCGSeedState::IsReadyForController(const APlayerController* PlayerController) const
{
	if (!PlayerController || !bServerGenerationComplete) { return false; }
	if (PlayerController->IsLocalController() && GetNetMode() != NM_DedicatedServer) { return true; }
	return ReadyControllers.FindRef(PlayerController) == State.Revision;
}

void AP48PCGSeedState::UpdateMapReady()
{
	if (!HasAuthority()) { return; }
	bool bReady = bServerGenerationComplete && (!bPlayerStartLayoutRequired || bPlayerStartLayoutValid);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); bReady && It; ++It)
	{
		bReady = IsReadyForController(It->Get());
	}
	if (State.bMapReady == bReady) { return; }
	State.bMapReady = bReady;
	ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("[P48MapReady] Ready=%d Revision=%d"), State.bMapReady, State.Revision);
	if (AP48GameModeBase* GameMode = GetWorld()->GetAuthGameMode<AP48GameModeBase>()) { GameMode->NotifyMapGenerationReadinessChanged(); }
}

void AP48PCGSeedState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (auto It = RegisteredPlayerStarts.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = Consumers.CreateIterator(); It; ++It)
	{
		UPCGComponent* Component = It.Key().Get();
		if (!Component) { It.RemoveCurrent(); continue; }
		if (State.Revision != 0 && It.Value() != State.Revision && !Component->IsGenerating())
		{
			It.Value() = State.Revision;
			CompletedConsumers.Remove(Component);
			Component->CleanupLocalImmediate(true, true);
			PendingGenerationConsumers.Add(Component);
		}
	}
	if (!PendingGenerationConsumers.IsEmpty() && !bConsumerGenerationScheduled)
	{
		bConsumerGenerationScheduled = true;
		GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::GeneratePendingConsumers);
	}
	if (HasAuthority())
	{
		RefreshPlayerStartReadiness();
		if (bServerGenerationComplete && bPlayerStartLayoutRequired && !bPlayerStartLayoutValid && !bPlayerStartTimeoutLogged && PlayerStartRegistrationDeadline > 0.0 && GetWorld()->GetTimeSeconds() >= PlayerStartRegistrationDeadline)
		{
			bPlayerStartTimeoutLogged = true;
			if (RequestedPlayerStartCount <= 0)
			{
				UE_LOG(LogTemp, Error, TEXT("[P48PlayerStartReady] Revision=%d did not receive a PlayerStart layout. Connect the Player Spawn Selector after the map generation barrier."), State.Revision);
			}
			else if (SelectedPlayerStartCount < RequestedPlayerStartCount)
			{
				UE_LOG(LogTemp, Error, TEXT("[P48PlayerStartReady] Revision=%d selected only %d of %d required PlayerStart candidates. Adjust safe-surface or spawn-distance settings."), State.Revision, SelectedPlayerStartCount, RequestedPlayerStartCount);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[P48PlayerStartReady] Revision=%d registered %d of %d required PlayerStarts after the registration timeout. Verify that Spawn Actor uses P48PlayerStart."), State.Revision, GetRegisteredPlayerStartCount(), RequestedPlayerStartCount);
			}
		}
	}
}

void AP48PCGSeedState::GeneratePendingConsumers()
{
	bConsumerGenerationScheduled = false;
	TSet<TWeakObjectPtr<UPCGComponent>> ConsumersToGenerate = MoveTemp(PendingGenerationConsumers);
	PendingGenerationConsumers.Reset();
	for (const TWeakObjectPtr<UPCGComponent>& WeakComponent : ConsumersToGenerate)
	{
		if (UPCGComponent* Component = WeakComponent.Get())
		{
			Component->GenerateLocal(true);
		}
	}
}

void AP48PCGSeedState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AP48PCGSeedState, State);
}
