#include "P48ItemSpawnPoint.h"

#include "P48ItemSpawnSubsystem.h"
#include "Project48/Weapon/P48WeaponBase.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AP48ItemSpawnPoint::AP48ItemSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ArrowComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrowComponent"));
	ArrowComponent->SetupAttachment(SceneRoot);
	ArrowComponent->SetArrowColor(FColor::Yellow);
	ArrowComponent->SetHiddenInGame(true);

	static ConstructorHelpers::FClassFinder<AP48WeaponBase> BatClass(
		TEXT("/Game/SJW/Weapon/BP/BP_Weapon_Bat"));
	static ConstructorHelpers::FClassFinder<AP48WeaponBase> PanClass(
		TEXT("/Game/SJW/Weapon/BP/BP_Weapon_Pan"));
	static ConstructorHelpers::FClassFinder<AP48WeaponBase> ToyHammerClass(
		TEXT("/Game/SJW/Weapon/BP/BP_Weapon_ToyHammer"));

	if (BatClass.Succeeded())
	{
		SpawnEntries.Add({BatClass.Class, 1.0f});
	}
	if (PanClass.Succeeded())
	{
		SpawnEntries.Add({PanClass.Class, 1.0f});
	}
	if (ToyHammerClass.Succeeded())
	{
		SpawnEntries.Add({ToyHammerClass.Class, 1.0f});
	}
}

void AP48ItemSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// PCG Spawn Actor의 Property Override가 모두 적용된 뒤 등록합니다.
		GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::RegisterWithSubsystem);
	}
}

void AP48ItemSpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bRegisteredWithSubsystem)
	{
		if (UP48ItemSpawnSubsystem* Subsystem = GetWorld()->GetSubsystem<UP48ItemSpawnSubsystem>())
		{
			Subsystem->UnregisterSpawnPoint(this);
		}
		bRegisteredWithSubsystem = false;
	}

	Super::EndPlay(EndPlayReason);
}

AP48WeaponBase* AP48ItemSpawnPoint::SpawnItem()
{
	if (!HasAuthority())
	{
		return nullptr;
	}

	if (UP48ItemSpawnSubsystem* Subsystem = GetWorld()->GetSubsystem<UP48ItemSpawnSubsystem>())
	{
		return Subsystem->SpawnAtPoint(this);
	}

	return nullptr;
}

FTransform AP48ItemSpawnPoint::GetItemSpawnTransform() const
{
	return FTransform(
		GetActorRotation(),
		GetActorLocation() + FVector::UpVector * FMath::Max(0.0f, SpawnHeight));
}

TSubclassOf<AP48WeaponBase> AP48ItemSpawnPoint::SelectWeaponClass(const int32 MapSeed) const
{
	double TotalWeight = 0.0;
	for (const FP48WeightedWeaponSpawnEntry& Entry : SpawnEntries)
	{
		if (Entry.WeaponClass && Entry.Weight > 0.0f)
		{
			TotalWeight += Entry.Weight;
		}
	}

	if (TotalWeight <= 0.0)
	{
		return nullptr;
	}

	const uint32 PointSeed = HashCombineFast(
		GetTypeHash(MapSeed),
		HashCombineFast(
			GetTypeHash(IslandIndex),
			HashCombineFast(GetTypeHash(SpawnSlotIndex), GetTypeHash(SeedOffset))));
	FRandomStream Random(static_cast<int32>(PointSeed));
	double Selection = Random.FRand() * TotalWeight;

	for (const FP48WeightedWeaponSpawnEntry& Entry : SpawnEntries)
	{
		if (!Entry.WeaponClass || Entry.Weight <= 0.0f)
		{
			continue;
		}

		Selection -= Entry.Weight;
		if (Selection <= 0.0)
		{
			return Entry.WeaponClass;
		}
	}

	for (int32 Index = SpawnEntries.Num() - 1; Index >= 0; --Index)
	{
		if (SpawnEntries[Index].WeaponClass && SpawnEntries[Index].Weight > 0.0f)
		{
			return SpawnEntries[Index].WeaponClass;
		}
	}

	return nullptr;
}

void AP48ItemSpawnPoint::RegisterWithSubsystem()
{
	if (!HasAuthority() || bRegisteredWithSubsystem)
	{
		return;
	}

	if (UP48ItemSpawnSubsystem* Subsystem = GetWorld()->GetSubsystem<UP48ItemSpawnSubsystem>())
	{
		bRegisteredWithSubsystem = Subsystem->RegisterSpawnPoint(this);
	}
}
