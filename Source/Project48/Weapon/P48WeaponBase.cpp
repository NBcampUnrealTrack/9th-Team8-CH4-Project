#include "P48WeaponBase.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"

AP48WeaponBase::AP48WeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;
	
	bReplicates = true;
	SetReplicateMovement(true);
	
	WeaponMeshComponent =
		CreateDefaultSubobject<UStaticMeshComponent>(
			TEXT("WeaponMeshComponent")
			);
	
	SetRootComponent(WeaponMeshComponent);
		
	WeaponMeshComponent->SetCollisionProfileName(
		UCollisionProfile::PhysicsActor_ProfileName
		);
	
	WeaponMeshComponent->SetSimulatePhysics(true);
	WeaponMeshComponent->SetEnableGravity(true);
	WeaponMeshComponent->SetNotifyRigidBodyCollision(true);
	WeaponMeshComponent->SetIsReplicated(true);
	
	WeaponMeshComponent->CanCharacterStepUpOn = ECB_No;
}

void AP48WeaponBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	ApplyWeaponData();
}

void AP48WeaponBase::BeginPlay()
{
	Super::BeginPlay();
	
	ApplyWeaponData();
}

void AP48WeaponBase::ApplyWeaponData()
{
	bHasValidWeaponData = false;
	WeaponMeshComponent->SetStaticMesh(nullptr);
	
	if (!WeaponDataHandle.DataTable)
	{
		return;
	}
	
	if (WeaponDataHandle.RowName.IsNone())
	{
		return;
	}
	
	const FWeaponDataRow* FoundData =
		WeaponDataHandle.GetRow<FWeaponDataRow>(
			TEXT("AP48WeaponBase::ApplyWeaponData")
			);
	
	if (!FoundData)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("%s: Weapon Data를 찾지 못했습니다. Row: %s"),
			*GetName(),
			*WeaponDataHandle.RowName.ToString()
			);
		
		return;
	}
	
	CachedWeaponData = *FoundData;
	bHasValidWeaponData = true;
	
	UStaticMesh* LoadedMesh = CachedWeaponData.WeaponMesh.LoadSynchronous();
	
	if (LoadedMesh)
	{
		WeaponMeshComponent->SetStaticMesh(LoadedMesh);
		
		const float SafeWeight = FMath::Max(CachedWeaponData.Weight, 0.01f);
		
		WeaponMeshComponent->SetMassOverrideInKg(
			NAME_None,
			SafeWeight,
			true
			);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s: Weapon Mesh가 비어 있습니다. Row: %s"),
			*GetName(),
			*WeaponDataHandle.RowName.ToString()
			);
	}
}

bool AP48WeaponBase::HasvalidWeaponData() const
{
	return bHasValidWeaponData;
}

FWeaponDataRow AP48WeaponBase::GetWeaponData() const
{
	return CachedWeaponData;
}

FName AP48WeaponBase::GetWeaponRowName() const
{
	return WeaponDataHandle.RowName;
}
