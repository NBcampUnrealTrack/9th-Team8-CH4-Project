#include "P48PlayerStartRegistrySubsystem.h"

#include "P48PlayerStart.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UP48PlayerStartRegistrySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RegistrationTimeoutHandle);
	}
	RegisteredPlayerStarts.Reset();
	OnReadinessChanged.Clear();
	Super::Deinitialize();
}

void UP48PlayerStartRegistrySubsystem::BeginGeneration(const int32 Revision, const int32 InRequiredCount)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RegistrationTimeoutHandle);
	}
	ActiveRevision = Revision;
	RequiredCount = FMath::Max(0, InRequiredCount);
	SelectedCount = INDEX_NONE;
	bRegistrationSealed = false;
	LayoutState = EP48PlayerStartLayoutState::Pending;
	RegisteredPlayerStarts.Reset();
}

void UP48PlayerStartRegistrySubsystem::UpdateRequiredCount(const int32 Revision, const int32 InRequiredCount)
{
	if (Revision != ActiveRevision || Revision <= 0)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RegistrationTimeoutHandle);
	}
	RequiredCount = FMath::Max(0, InRequiredCount);
	EvaluateReadiness();
	if (bRegistrationSealed && RequiredCount > 0 && LayoutState == EP48PlayerStartLayoutState::Pending)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(RegistrationTimeoutHandle, this, &ThisClass::HandleRegistrationTimeout, RegistrationGraceSeconds, false);
		}
	}
}

void UP48PlayerStartRegistrySubsystem::ReportSelectedLayout(const int32 Revision, const int32 InSelectedCount)
{
	if (Revision != ActiveRevision || Revision <= 0)
	{
		return;
	}
	SelectedCount = FMath::Max(0, InSelectedCount);
	EvaluateReadiness();
	UE_LOG(LogTemp, Display, TEXT("[P48PlayerStartLayout] Revision=%d Required=%d Selected=%d Registered=%d"), ActiveRevision, RequiredCount, SelectedCount, GetRegisteredCount(ActiveRevision));
}

void UP48PlayerStartRegistrySubsystem::RegisterPlayerStart(AP48PlayerStart* PlayerStart)
{
	if (!IsValid(PlayerStart) || !PlayerStart->HasAuthority() || ActiveRevision <= 0)
	{
		return;
	}

	PruneInvalidStarts();
	TSet<int32> UsedSlots;
	for (const TWeakObjectPtr<AP48PlayerStart>& ExistingStart : RegisteredPlayerStarts)
	{
		const AP48PlayerStart* Existing = ExistingStart.Get();
		if (Existing && Existing != PlayerStart && Existing->GenerationId == ActiveRevision && Existing->SpawnSlotIndex != INDEX_NONE)
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

	PlayerStart->GenerationId = ActiveRevision;
	RegisteredPlayerStarts.Add(PlayerStart);
	EvaluateReadiness();
	UE_LOG(LogTemp, Display, TEXT("[P48PlayerStartRegister] Revision=%d Name=%s Slot=%d Island=%d Registered=%d/%d"), ActiveRevision, *GetNameSafe(PlayerStart), PlayerStart->SpawnSlotIndex, PlayerStart->IslandIndex, GetRegisteredCount(ActiveRevision), RequiredCount);
}

void UP48PlayerStartRegistrySubsystem::UnregisterPlayerStart(AP48PlayerStart* PlayerStart)
{
	if (!PlayerStart)
	{
		return;
	}
	RegisteredPlayerStarts.Remove(PlayerStart);
	EvaluateReadiness();
}

void UP48PlayerStartRegistrySubsystem::SealRegistration(const int32 Revision)
{
	if (Revision != ActiveRevision || Revision <= 0)
	{
		return;
	}
	bRegistrationSealed = true;
	EvaluateReadiness();
	if (RequiredCount > 0 && LayoutState == EP48PlayerStartLayoutState::Pending)
	{
		GetWorld()->GetTimerManager().SetTimer(RegistrationTimeoutHandle, this, &ThisClass::HandleRegistrationTimeout, RegistrationGraceSeconds, false);
	}
}

EP48PlayerStartLayoutState UP48PlayerStartRegistrySubsystem::GetState(const int32 Revision) const
{
	return Revision == ActiveRevision ? LayoutState : EP48PlayerStartLayoutState::Pending;
}

int32 UP48PlayerStartRegistrySubsystem::GetRegisteredCount(const int32 Revision) const
{
	if (Revision != ActiveRevision)
	{
		return 0;
	}
	TSet<int32> UniqueSlots;
	for (const TWeakObjectPtr<AP48PlayerStart>& WeakStart : RegisteredPlayerStarts)
	{
		const AP48PlayerStart* PlayerStart = WeakStart.Get();
		if (PlayerStart && PlayerStart->GenerationId == ActiveRevision && PlayerStart->SpawnSlotIndex != INDEX_NONE)
		{
			UniqueSlots.Add(PlayerStart->SpawnSlotIndex);
		}
	}
	return UniqueSlots.Num();
}

void UP48PlayerStartRegistrySubsystem::EvaluateReadiness()
{
	PruneInvalidStarts();
	const bool bReady = RequiredCount > 0 && SelectedCount >= RequiredCount && GetRegisteredCount(ActiveRevision) >= RequiredCount;
	if (bReady)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RegistrationTimeoutHandle);
		}
		SetLayoutState(EP48PlayerStartLayoutState::Ready);
	}
	else if (LayoutState == EP48PlayerStartLayoutState::Ready)
	{
		SetLayoutState(EP48PlayerStartLayoutState::Pending);
	}
}

void UP48PlayerStartRegistrySubsystem::HandleRegistrationTimeout()
{
	if (LayoutState != EP48PlayerStartLayoutState::Pending || !bRegistrationSealed)
	{
		return;
	}
	SetLayoutState(EP48PlayerStartLayoutState::Failed);
	UE_LOG(LogTemp, Error, TEXT("[P48PlayerStartReady] Revision=%d selected %d and registered %d of %d required PlayerStarts."), ActiveRevision, FMath::Max(0, SelectedCount), GetRegisteredCount(ActiveRevision), RequiredCount);
}

void UP48PlayerStartRegistrySubsystem::SetLayoutState(const EP48PlayerStartLayoutState NewState)
{
	if (LayoutState == NewState)
	{
		return;
	}
	LayoutState = NewState;
	OnReadinessChanged.Broadcast(ActiveRevision);
}

void UP48PlayerStartRegistrySubsystem::PruneInvalidStarts()
{
	for (auto It = RegisteredPlayerStarts.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
