#include "P48PlayerStart.h"

#include "P48PlayerStartRegistrySubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"

void AP48PlayerStart::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::RegisterWithRegistry);
	}
}

void AP48PlayerStart::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bRegisteredWithRegistry)
	{
		if (UP48PlayerStartRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UP48PlayerStartRegistrySubsystem>())
		{
			Registry->UnregisterPlayerStart(this);
		}
		bRegisteredWithRegistry = false;
	}
	Super::EndPlay(EndPlayReason);
}

void AP48PlayerStart::RegisterWithRegistry()
{
	if (!HasAuthority() || bRegisteredWithRegistry)
	{
		return;
	}
	if (UP48PlayerStartRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UP48PlayerStartRegistrySubsystem>())
	{
		Registry->RegisterPlayerStart(this);
		bRegisteredWithRegistry = GenerationId > 0;
	}
}
