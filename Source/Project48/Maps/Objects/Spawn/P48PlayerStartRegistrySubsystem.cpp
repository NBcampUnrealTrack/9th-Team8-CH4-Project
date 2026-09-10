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
	SelectedStarts.Reset();
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
	SelectedStarts.Reset();

	// PCG owns actor cleanup. Clearing this registry prevents old starts from
	// being claimed without destroying manually placed actors of the same class.
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

void UP48PlayerStartRegistrySubsystem::ReportSelectedStarts(const int32 Revision, const TArray<FP48SelectedPlayerStart>& Starts)
{
	if (Revision != ActiveRevision || Revision <= 0 || SelectedCount != INDEX_NONE) { return; }
	SelectedStarts = Starts;
	ReportSelectedLayout(Revision, SelectedStarts.Num());
}

bool UP48PlayerStartRegistrySubsystem::RegisterPlayerStart(AP48PlayerStart* PlayerStart)
{
	if (!IsValid(PlayerStart) || PlayerStart->GetWorld() != GetWorld() || !PlayerStart->HasAuthority() || ActiveRevision <= 0)
	{
		return false;
	}
	if (RegisteredPlayerStarts.Contains(PlayerStart)) { return true; }

	// Selector가 현재 세대 레이아웃을 보고하기 전에 BeginPlay한 Actor는 이전 PCG 결과다.
	if (SelectedCount == INDEX_NONE)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[P48PlayerStartRegister] Rejected pre-layout start Name=%s Revision=%d"),
			*GetNameSafe(PlayerStart), ActiveRevision);
		return false;
	}

	if (PlayerStart->GenerationId > 0 && PlayerStart->GenerationId != ActiveRevision)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[P48PlayerStartRegister] Rejected stale start Name=%s ActorRevision=%d ActiveRevision=%d"),
			*GetNameSafe(PlayerStart), PlayerStart->GenerationId, ActiveRevision);
		return false;
	}

	if (SelectedCount <= 0 || GetRegisteredCount(ActiveRevision) >= SelectedCount)
	{
		return false;
	}

	PruneInvalidStarts();
	const int32 Slot = SelectedStarts.IndexOfByPredicate([PlayerStart](const FP48SelectedPlayerStart& Start)
	{
		return Start.IslandIndex >= 0 && Start.Location.Equals(PlayerStart->GetActorLocation(), 1.0);
	});
	if (Slot == INDEX_NONE) { return false; }
	if ((PlayerStart->SpawnSlotIndex != INDEX_NONE && PlayerStart->SpawnSlotIndex != Slot)
		|| (PlayerStart->IslandIndex != INDEX_NONE && PlayerStart->IslandIndex != SelectedStarts[Slot].IslandIndex)) { return false; }
	for (const TWeakObjectPtr<AP48PlayerStart>& WeakStart : RegisteredPlayerStarts)
	{
		if (WeakStart.IsValid() && WeakStart->SpawnSlotIndex == Slot) { return false; }
	}
	// Defaults may be filled only after matching the actual selected location.
	PlayerStart->SpawnSlotIndex = Slot;
	PlayerStart->IslandIndex = SelectedStarts[Slot].IslandIndex;
	PlayerStart->GenerationId = ActiveRevision;
	RegisteredPlayerStarts.Add(PlayerStart);
	EvaluateReadiness();
	UE_LOG(LogTemp, Display, TEXT("[P48PlayerStartRegister] Revision=%d Name=%s Slot=%d Island=%d Registered=%d/%d"), ActiveRevision, *GetNameSafe(PlayerStart), PlayerStart->SpawnSlotIndex, PlayerStart->IslandIndex, GetRegisteredCount(ActiveRevision), RequiredCount);
	return true;
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
