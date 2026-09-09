#include "P48PlayerStart.h"

#include "../../PCG/Common/P48PCGSeedState.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace P48PlayerStartRegistration
{
	constexpr int32 MaxAttempts = 60;
}

void AP48PlayerStart::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::RegisterWithSeedState);
	}
}

void AP48PlayerStart::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RegisteredSeedState.IsValid())
	{
		RegisteredSeedState->UnregisterGeneratedPlayerStart(this);
		RegisteredSeedState.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void AP48PlayerStart::RegisterWithSeedState()
{
	if (!HasAuthority() || RegisteredSeedState.IsValid())
	{
		return;
	}

	for (TActorIterator<AP48PCGSeedState> It(GetWorld()); It; ++It)
	{
		AP48PCGSeedState* SeedState = *It;
		if (SeedState->State.Revision <= 0)
		{
			continue;
		}

		RegisteredSeedState = SeedState;
		SeedState->RegisterGeneratedPlayerStart(this);
		return;
	}

	++RegistrationAttempts;
	if (RegistrationAttempts < P48PlayerStartRegistration::MaxAttempts)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::RegisterWithSeedState);
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("[P48PlayerStartReady] %s could not find a valid P48PCGSeedState after %d frames."), *GetName(), RegistrationAttempts);
}
