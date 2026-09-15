#include "P48WeaponBase.h"

#include "Project48/Character/P48PlayerCharacter.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Components/BoxComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogP48Weapon, Log, All);

AP48WeaponBase::AP48WeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
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
	
	// Construction 단계에서 적용되지 않은 경우에만 다시 시도
	if (!bHasValidWeaponData)
	{
		ApplyWeaponData();
	}
}

void AP48WeaponBase::ApplyWeaponData()
{
	bHasValidWeaponData = false;
	CachedSwingSound = nullptr;
	CachedHitSound = nullptr;
	WeaponMeshComponent->SetStaticMesh(nullptr);
	
	if (!WeaponDataHandle.DataTable)
	{
		UE_LOG(LogP48Weapon, Error, TEXT("%s: Weapon DataTable이 설정되지 않았습니다."), *GetName());
		return;
	}
	
	if (WeaponDataHandle.RowName.IsNone())
	{
		UE_LOG(LogP48Weapon, Error, TEXT("%s: Weapon DataTable Row가 설정되지 않았습니다."), *GetName());
		return;
	}
	
	const FWeaponDataRow* FoundData =
		WeaponDataHandle.GetRow<FWeaponDataRow>(
			TEXT("AP48WeaponBase::ApplyWeaponData")
			);
	
	if (!FoundData)
	{
		UE_LOG(
			LogP48Weapon,
			Error,
			TEXT("%s: Weapon Data를 찾지 못했습니다. Row: %s"),
			*GetName(),
			*WeaponDataHandle.RowName.ToString()
			);
		
		return;
	}
	
	if (!FoundData->HasValidBalanceData())
	{
		UE_LOG(
			LogP48Weapon,
			Error,
			TEXT("%s: Weapon 수치 데이터가 올바르지 않습니다. "
				"Row=%s, GroggyDamage=%.1f, KnockbackPower=%.1f, "
				"AttackCooldown=%.1f, Weight=%.1f"),
				*GetName(),
				*WeaponDataHandle.RowName.ToString(),
				FoundData->GroggyDamage,
				FoundData->KnockbackPower,
				FoundData->AttackCooldown,
				FoundData->Weight);
		
		return;
	}
	
	CachedWeaponData = *FoundData;
	CachedSwingSound = CachedWeaponData.SwingSound.LoadSynchronous();
	CachedHitSound = CachedWeaponData.HitSound.LoadSynchronous();
		
	UStaticMesh* LoadedMesh = CachedWeaponData.WeaponMesh.LoadSynchronous();
	
	if (LoadedMesh)
	{
		WeaponMeshComponent->SetStaticMesh(LoadedMesh);
		
		const float SafeWeight = FMath::Max(CachedWeaponData.Weight, 0.01f);
		
		WeaponMeshComponent->SetMassOverrideInKg(
			NAME_None,
			SafeWeight,
			true);
			
		bHasValidWeaponData = true;
	}
	else
	{
		UE_LOG(
			LogP48Weapon,
			Warning,
			TEXT("%s: Weapon Mesh가 비어 있습니다. Row: %s"),
			*GetName(),
			*WeaponDataHandle.RowName.ToString()
			);
	}
}

bool AP48WeaponBase::HasValidWeaponData() const
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
	
	bIsAttackDetectionActive = true;
	
	AttackCollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	
	UE_LOG(LogP48Weapon, Log, TEXT("%s 무기 공격 판정 시작"), *GetName());
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
	
	UE_LOG(LogP48Weapon, Log, TEXT("%s 무기 공격 판정 종료"), *GetName());
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
	
	if (bHasHitActorThisAttack)
	{
		return;
	}
	
	if (HandleWeaponHit(OtherActor))
	{
		bHasHitActorThisAttack = true;
		
		UE_LOG(LogP48Weapon, Log, TEXT("%s: 공격 대상 감지 [%s]"), *GetName(), *OtherActor->GetName());
	}
}

bool AP48WeaponBase::HandleWeaponHit(AActor* HitActor)
{
	// 서버에서만 무기 피격 처리
	if (!HasAuthority())
	{
		return false;
	}
	
	// 유효하지 않은 피격 대상 제외
	if (!IsValid(HitActor))
	{
		return false;
	}
	
	// 무기 DT와 그로기 GE 설정 검증
	if (!bHasValidWeaponData)
	{
		UE_LOG(LogP48Weapon, Warning, TEXT("%s: 유효한 무기가 없어 피격을 처리할 수 없습니다."), *GetName());
		
		return false;
	}
	
	if (!GroggyDamageEffectClass)
	{
		UE_LOG(LogP48Weapon, Warning, TEXT("%s GroggyDamageEffectClass가 설정되지 않았습니다."), *GetName());
		
		return false;
	}
	
	// 피격 대상 ASC 조회. ASC가 없는 오브젝트 공격 대상 제외
	UAbilitySystemComponent* TargetASC = 
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	
	if (!TargetASC)
	{
		UE_LOG(
			LogP48Weapon,
			Verbose,
			TEXT("%s: 대상 %s에 ASC가 없어 피격을 무시합니다."),
			*GetName(),
			*HitActor->GetName());
		
		return false;
	}
	
	// 플레이어 피격 함수에 전달할 넉백 정보
	const FVector KnockbackDirection = CalculateKnockbackDirection(HitActor);
	
	const float KnockbackPower = FMath::Max(CachedWeaponData.KnockbackPower, 0.0f);
	
	UE_LOG(
		LogP48Weapon,
		Log,
		TEXT("%s: Knockback Target=%s, Direction=%s, Power=%.1f"),
		*GetName(),
		*HitActor->GetName(),
		*KnockbackDirection.ToString(),
		KnockbackPower);
	
	// 무기 DT의 GroggyDamage를 전달할 GE Spec 생성
	FGameplayEffectContextHandle EffectContext = TargetASC->MakeEffectContext();
	
	EffectContext.AddSourceObject(this);
	
	FGameplayEffectSpecHandle EffectSpecHandle =
		TargetASC->MakeOutgoingSpec(
			GroggyDamageEffectClass,
			1.0f,
			EffectContext);
	
	if (!EffectSpecHandle.IsValid())
	{
		UE_LOG(LogP48Weapon, Error, TEXT("%s Groggy GameplayEffect Spec 생성에 실패했습니다."), *GetName());
		
		return false;
	}
	
	static const FGameplayTag GroggyDamageTag =
		FGameplayTag::RequestGameplayTag(FName(TEXT("Data.GroggyDamage")));
	
	// GE_GroggyDamage의 Data.GroggyDamage에 무기별 수치 전달
	EffectSpecHandle.Data->SetSetByCallerMagnitude(GroggyDamageTag, CachedWeaponData.GroggyDamage);
	
	// 대상 ASC에 그로기 GE 적용
	TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());

	// 타격 판정이 서버에서 성공한 경우에만 모든 클라이언트에서 재생한다.
	Multicast_PlayHitSound(HitActor->GetActorLocation());
	
	if (AP48PlayerCharacter* TargetCharacter = Cast<AP48PlayerCharacter>(HitActor))
	{
		TargetCharacter->OnHit(TargetCharacter->GetActorLocation(), KnockbackDirection, KnockbackPower);
	}
	
	UE_LOG(
		LogP48Weapon,
		Log,
		TEXT("%s: %s에게 GroggyDamage %.1f 적용"),
		*GetName(),
		*HitActor->GetName(),
		CachedWeaponData.GroggyDamage);
	
	return true;
}

void AP48WeaponBase::PlaySwingSound()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!CachedSwingSound && !CachedWeaponData.SwingSound.IsNull())
	{
		CachedSwingSound = CachedWeaponData.SwingSound.LoadSynchronous();
	}

	if (CachedSwingSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, CachedSwingSound, GetActorLocation());
	}
}

void AP48WeaponBase::Multicast_PlayHitSound_Implementation(FVector_NetQuantize HitLocation)
{
	PlayHitSoundAtLocation(HitLocation);
}

void AP48WeaponBase::PlayHitSoundAtLocation(const FVector& HitLocation)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!CachedHitSound && !CachedWeaponData.HitSound.IsNull())
	{
		CachedHitSound = CachedWeaponData.HitSound.LoadSynchronous();
	}

	if (CachedHitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, CachedHitSound, HitLocation);
	}
}

FVector AP48WeaponBase::CalculateKnockbackDirection(const AActor* HitActor) const
{
	if (!IsValid(HitActor))
	{
		return FVector::ZeroVector;
	}
	
	// 무기 소유자가 있으면 공격자 기준 계산, 없으면 무기 위치 기준 계산(테스트용)
	const AActor* WeaponOwner = GetOwner();
	const AActor* KnockbackSource = IsValid(WeaponOwner) ? WeaponOwner : this;
	
	FVector KnockbackDirection = HitActor->GetActorLocation() - KnockbackSource->GetActorLocation();
	
	return KnockbackDirection.GetSafeNormal2D();
}

void AP48WeaponBase::ResetAttackState()
{
	if (HasAuthority())
	{
		bHasHitActorThisAttack = false;
	}
}

void AP48WeaponBase::OnDropped(const FVector& DropImpulse)
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetOwner(nullptr);
	
	if (UStaticMeshComponent* WeaponMesh = FindComponentByClass<UStaticMeshComponent>())
	{
		WeaponMesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		WeaponMesh->SetSimulatePhysics(true);
		WeaponMesh->SetEnableGravity(true);
		
		if (!DropImpulse.IsNearlyZero())
		{
			WeaponMesh->AddImpulse(DropImpulse, NAME_None, true);
		}
	}
}
