#include "P48WeaponBase.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Components/BoxComponent.h"

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
		
	AttackCollisionComponent = CreateDefaultSubobject<UBoxComponent>(
		TEXT("AttackCollisionComponent"));
	
	AttackCollisionComponent->SetupAttachment(WeaponMeshComponent);
	
	AttackCollisionComponent->SetBoxExtent(FVector(15.0f, 15.0f, 50.0f));
	
	AttackCollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AttackCollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	AttackCollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AttackCollisionComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	AttackCollisionComponent->SetGenerateOverlapEvents(true);
	AttackCollisionComponent->OnComponentBeginOverlap.AddDynamic(
		this,
		&AP48WeaponBase::OnAttackCollisionBeginOverlap);
	
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

void AP48WeaponBase::StartAttackDetection()
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (bIsAttackDetectionActive)
	{
		return;
	}
	
	HitActorsThisAttack.Reset();
	bIsAttackDetectionActive = true;
	
	AttackCollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	
	UE_LOG(LogTemp, Log, TEXT("%s 무기 공격 판정 시작"), *GetName());
}

void AP48WeaponBase::StopAttackDetection()
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (!bIsAttackDetectionActive)
	{
		return;
	}
	
	bIsAttackDetectionActive = false;
	
	AttackCollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	UE_LOG(LogTemp, Log, TEXT("%s 무기 공격 판정 종료"), *GetName());
}

void AP48WeaponBase::OnAttackCollisionBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (!bIsAttackDetectionActive)
	{
		return;
	}
	
	if (OtherActor == this || OtherActor == GetOwner())
	{
		return;
	}
	
	const TWeakObjectPtr<AActor> HitActor(OtherActor);
	
	if (HitActorsThisAttack.Contains(HitActor))
	{
		return;
	}
	
	HitActorsThisAttack.Add(HitActor);
	
	HandleWeaponHit(OtherActor);
	
	UE_LOG(LogTemp, Log, TEXT("%s: 공격 대상 감지 [%s]"), *GetName(), *OtherActor->GetName());
}

void AP48WeaponBase::HandleWeaponHit(AActor* HitActor)
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (!IsValid(HitActor))
	{
		return;
	}
	
	if (!bHasValidWeaponData)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 유효한 무기가 없어 피격을 처리할 수 없습니다."), *GetName());
		
		return;
	}
	
	UE_LOG(LogTemp,
		Log,
		TEXT("%s: 공격 대상=%s, Row=%s, GroggyDamage=%.1f,"
		"KnockbackPower=%1f"),
		*GetName(),
		*HitActor->GetName(),
		*WeaponDataHandle.RowName.ToString(),
		CachedWeaponData.GroggyDamage,
		CachedWeaponData.KnockbackPower);
}
