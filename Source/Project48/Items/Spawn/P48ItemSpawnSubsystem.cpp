#include "P48ItemSpawnSubsystem.h"

#include "P48ItemSpawnPoint.h"
#include "Project48/Maps/PCG/Common/P48PCGSeedWorldSubsystem.h"
#include "Project48/Weapon/P48WeaponBase.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogP48ItemSpawn, Log, All);

void UP48ItemSpawnSubsystem::Deinitialize()
{
	SpawnedItems.Reset();
	Super::Deinitialize();
}

bool UP48ItemSpawnSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UP48ItemSpawnSubsystem::RegisterSpawnPoint(AP48ItemSpawnPoint* SpawnPoint)
{
	UWorld* World = GetWorld();
	if (!IsValid(SpawnPoint) || !World || World->GetNetMode() == NM_Client)
	{
		return false;
	}

	if (!IsCurrentGeneration(SpawnPoint))
	{
		UE_LOG(
			LogP48ItemSpawn,
			Warning,
			TEXT("오래된 ItemSpawnPoint 등록을 거부했습니다. Point=%s Generation=%d"),
			*GetNameSafe(SpawnPoint),
			SpawnPoint->GenerationId);
		return false;
	}

	SpawnedItems.FindOrAdd(SpawnPoint);
	if (SpawnPoint->bAutoSpawn)
	{
		SpawnAtPoint(SpawnPoint);
	}

	return true;
}

void UP48ItemSpawnSubsystem::UnregisterSpawnPoint(AP48ItemSpawnPoint* SpawnPoint)
{
	if (!SpawnPoint)
	{
		return;
	}

	if (const TWeakObjectPtr<AP48WeaponBase>* SpawnedItem = SpawnedItems.Find(SpawnPoint))
	{
		AP48WeaponBase* Weapon = SpawnedItem->Get();
		if (IsValid(Weapon) && !Weapon->GetOwner() && !Weapon->GetAttachParentActor())
		{
			// PCG 맵 재생성 시 이전 라운드에 줍지 않은 무기만 함께 정리합니다.
			Weapon->Destroy();
		}
	}

	SpawnedItems.Remove(SpawnPoint);
}

AP48WeaponBase* UP48ItemSpawnSubsystem::SpawnAtPoint(AP48ItemSpawnPoint* SpawnPoint)
{
	UWorld* World = GetWorld();
	if (!IsValid(SpawnPoint) || !World || World->GetNetMode() == NM_Client || !IsCurrentGeneration(SpawnPoint))
	{
		return nullptr;
	}

	if (const TWeakObjectPtr<AP48WeaponBase>* ExistingItem = SpawnedItems.Find(SpawnPoint))
	{
		if (ExistingItem->IsValid())
		{
			return ExistingItem->Get();
		}
	}

	const TSubclassOf<AP48WeaponBase> WeaponClass = SpawnPoint->SelectWeaponClass(ResolveMapSeed());
	if (!WeaponClass)
	{
		UE_LOG(LogP48ItemSpawn, Warning, TEXT("%s에 유효한 무기 스폰 항목이 없습니다."), *GetNameSafe(SpawnPoint));
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	AP48WeaponBase* SpawnedWeapon = World->SpawnActor<AP48WeaponBase>(
		WeaponClass,
		SpawnPoint->GetItemSpawnTransform(),
		SpawnParameters);
	if (!SpawnedWeapon)
	{
		UE_LOG(
			LogP48ItemSpawn,
			Warning,
			TEXT("아이템 생성에 실패했습니다. Point=%s Island=%d Class=%s"),
			*GetNameSafe(SpawnPoint),
			SpawnPoint->IslandIndex,
			*GetNameSafe(WeaponClass));
		return nullptr;
	}

	SpawnedItems.FindOrAdd(SpawnPoint) = SpawnedWeapon;
	UE_LOG(
		LogP48ItemSpawn,
		Log,
		TEXT("아이템을 생성했습니다. Point=%s Island=%d Item=%s"),
		*GetNameSafe(SpawnPoint),
		SpawnPoint->IslandIndex,
		*GetNameSafe(SpawnedWeapon));
	return SpawnedWeapon;
}

AP48WeaponBase* UP48ItemSpawnSubsystem::GetSpawnedItem(const AP48ItemSpawnPoint* SpawnPoint) const
{
	if (const TWeakObjectPtr<AP48WeaponBase>* Item = SpawnedItems.Find(SpawnPoint))
	{
		return Item->Get();
	}
	return nullptr;
}

int32 UP48ItemSpawnSubsystem::ResolveMapSeed() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const UP48PCGSeedWorldSubsystem* Coordinator = World->GetSubsystem<UP48PCGSeedWorldSubsystem>())
		{
			return Coordinator->GetGenerationContext().Seed;
		}
	}
	return 0;
}

bool UP48ItemSpawnSubsystem::IsCurrentGeneration(const AP48ItemSpawnPoint* SpawnPoint) const
{
	if (!SpawnPoint)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const UP48PCGSeedWorldSubsystem* Coordinator =
		World ? World->GetSubsystem<UP48PCGSeedWorldSubsystem>() : nullptr;
	const int32 CurrentGenerationId = Coordinator
		? Coordinator->GetGenerationContext().GenerationId
		: 0;

	return SpawnPoint->GenerationId <= 0 ||
		CurrentGenerationId <= 0 ||
		SpawnPoint->GenerationId == CurrentGenerationId;
}
