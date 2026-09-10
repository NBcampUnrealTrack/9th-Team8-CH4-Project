#include "P48PCGSeedState.h"

#include "P48PCGSeedWorldSubsystem.h"
#include "../../Objects/Spawn/P48PlayerStartRegistrySubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

AP48PCGSeedState::AP48PCGSeedState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bNetLoadOnClient = false;
	PrimaryActorTick.bCanEverTick = false;
}

void AP48PCGSeedState::BeginPlay()
{
	Super::BeginPlay();
	if (UP48PCGSeedWorldSubsystem* Coordinator = GetWorld()->GetSubsystem<UP48PCGSeedWorldSubsystem>())
	{
		Coordinator->SetReplicatedState(this);
	}
}

void AP48PCGSeedState::SetGenerationSnapshot(const FP48PCGGenerationSnapshot& InSnapshot)
{
	if (!HasAuthority())
	{
		return;
	}
	State = InSnapshot;
	ForceNetUpdate();
	if (UP48PCGSeedWorldSubsystem* Coordinator = GetWorld()->GetSubsystem<UP48PCGSeedWorldSubsystem>())
	{
		Coordinator->HandleReplicatedSnapshot(State);
	}
}

void AP48PCGSeedState::SetMapSeed(const int32 Seed)
{
	if (UP48PCGSeedWorldSubsystem* Coordinator = GetWorld()->GetSubsystem<UP48PCGSeedWorldSubsystem>())
	{
		Coordinator->RequestGeneration(FMath::Max(1, State.RequiredPlayerCount), Seed);
	}
}

void AP48PCGSeedState::RegisterConsumer(UPCGComponent* Component)
{
	if (UP48PCGSeedWorldSubsystem* Coordinator = GetWorld()->GetSubsystem<UP48PCGSeedWorldSubsystem>())
	{
		Coordinator->RegisterConsumer(Component);
	}
}

void AP48PCGSeedState::ReportPlayerStartLayout(const int32, const int32 SelectedCount)
{
	if (UP48PlayerStartRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UP48PlayerStartRegistrySubsystem>())
	{
		Registry->ReportSelectedLayout(State.Revision, SelectedCount);
	}
}

void AP48PCGSeedState::RegisterGeneratedPlayerStart(AP48PlayerStart* PlayerStart)
{
	if (UP48PlayerStartRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UP48PlayerStartRegistrySubsystem>())
	{
		Registry->RegisterPlayerStart(PlayerStart);
	}
}

void AP48PCGSeedState::UnregisterGeneratedPlayerStart(AP48PlayerStart* PlayerStart)
{
	if (UP48PlayerStartRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UP48PlayerStartRegistrySubsystem>())
	{
		Registry->UnregisterPlayerStart(PlayerStart);
	}
}

void AP48PCGSeedState::NotifyClientGenerationComplete(APlayerController* PlayerController, const int32 Revision)
{
	if (UP48PCGSeedWorldSubsystem* Coordinator = GetWorld()->GetSubsystem<UP48PCGSeedWorldSubsystem>())
	{
		Coordinator->NotifyClientGenerationComplete(PlayerController, Revision);
	}
}

void AP48PCGSeedState::NotifyControllerJoined(APlayerController* PlayerController)
{
	if (UP48PCGSeedWorldSubsystem* Coordinator = GetWorld()->GetSubsystem<UP48PCGSeedWorldSubsystem>())
	{
		Coordinator->NotifyControllerJoined(PlayerController);
	}
}

bool AP48PCGSeedState::IsReadyForController(const APlayerController* PlayerController) const
{
	if (const UP48PCGSeedWorldSubsystem* Coordinator = GetWorld()->GetSubsystem<UP48PCGSeedWorldSubsystem>())
	{
		return Coordinator->IsReadyForController(PlayerController);
	}
	return false;
}

void AP48PCGSeedState::OnRep_State()
{
	if (UP48PCGSeedWorldSubsystem* Coordinator = GetWorld()->GetSubsystem<UP48PCGSeedWorldSubsystem>())
	{
		Coordinator->SetReplicatedState(this);
	}
}

void AP48PCGSeedState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AP48PCGSeedState, State);
}
