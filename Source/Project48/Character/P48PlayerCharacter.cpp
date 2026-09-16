#include "P48PlayerCharacter.h"

#include "Project48/GAS/P48GroggyAttributeSet.h"
#include "Project48/DataTable/CharacterStatDataTypes.h"
#include "Project48/Effect/P48GE_Run.h"
#include "Project48/Effect/P48GE_Damage.h"
#include "Project48/UI/P48PlayerNameWidgetComponent.h"
#include "Project48/UI/P48PlayerNameWidget.h"
#include "Project48/Character/P48PlayerState.h"
#include "Project48/Weapon/P48WeaponBase.h"

#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SphereComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

AP48PlayerCharacter::AP48PlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->TargetArmLength = 450.0f; //
	SpringArm->SetRelativeRotation(FRotator(-45.0f, 0.0f, 0.0f)); //
	
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->SetRelativeLocation(FVector::ZeroVector);
	Camera->SetRelativeRotation(FRotator::ZeroRotator);
	
	
	//컨트롤러 회전 동기화 해제
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	
	//컨트롤러 방향으로 회전 X
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	//이동방향으로 몸 돌리기 
	GetCharacterMovement()->bOrientRotationToMovement = true;
	//몸통 회전 속도 (540.f -> 초당 540도 돌리기)
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	
	GetCapsuleComponent()->InitCapsuleSize(34.f, 65.f);
	
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -65.f));
	GetMesh()->bEnablePhysicsOnDedicatedServer = true;
	
	//GAS
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	GroggyAttributeSet = CreateDefaultSubobject<UP48GroggyAttributeSet>(TEXT("GroggyAttributeSet"));
	
	//공중에서 이동 제어
	GetCharacterMovement()->AirControl = 0.2f;
	GetCharacterMovement()->FallingLateralFriction = 1.0f;
	GetCharacterMovement()->BrakingDecelerationFalling = 300.f;
	
	GetCharacterMovement()->BrakingDecelerationWalking = 5000.f;
	
	WalkSpeedMultiplier = 1.5f;
	bIsSprint = false;
	DefaultWalkSpeed = 5000.f; //DT에서 제대로 불러오는지 확인하기 위한 임시 조정
	
	GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed;
	GetCharacterMovement()->MaxAcceleration = 5000.f;
	GetCharacterMovement()->GroundFriction = 10.f;
	GetCharacterMovement()->MaxStepHeight = 10.f;
	
	static ConstructorHelpers::FObjectFinder<USoundBase> JumpSoundFinder(TEXT("/Game/OJH/Resource/Sound/cartoon_jump.cartoon_jump"));
	if (JumpSoundFinder.Succeeded())
	{
		JumpSound = JumpSoundFinder.Object;
	}
	
	//오른손 소켓
	RightHandHitboxOffset = FVector(0.0f, 0.0f, 0.0f);
	RightHandHitboxRadius = 12.0f;
	
	RightHandHitbox = CreateDefaultSubobject<USphereComponent>(TEXT("RightHandSocket"));
	RightHandHitbox->SetupAttachment(GetMesh(), TEXT("handslot_r"));
	
	RightHandHitbox->SetAbsolute(false, false, true);
	RightHandHitbox->SetSphereRadius(RightHandHitboxRadius);
	
	RightHandHitbox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RightHandHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	RightHandHitbox->SetGenerateOverlapEvents(true);
	
	RunEffectClass = UP48GE_Run::StaticClass();
	
	//UI
	NicknameWidgetComponent = CreateDefaultSubobject<UP48PlayerNameWidgetComponent>(TEXT("NicknameWidgetComponent"));
	NicknameWidgetComponent->SetupAttachment(RootComponent);
	NicknameWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	NicknameWidgetComponent->SetUsingAbsoluteRotation(true);
	
	//Carry Settings
	CarrySocketName = TEXT("handslot_r");
	ThrowForwardSpeed = 1800.0f;
	ThrowUpSpeed = 600.0f;
	RagdollRecoverDuration = 1.5f;
	bIsInThrownRagdoll = false;
	CarriedForwardOffset = 15.0f;
	CarriedRightOffset = 0.0f;
	CarriedHeightOffset = -40.0f;
	CarriedYawOffset = 0.0f;
	bEnableCarriedRagdoll = true;
	CarriedPhysicsBlendWeight = 0.6f;
	CarriedPhysicsBoneName = TEXT("spine");
	bEnableCarryDebug = true;
}

void AP48PlayerCharacter::Destroyed()
{
	if (HasAuthority())
	{
		if (Weapon)
		{
			Weapon->Destroy();
			Weapon = nullptr;
		}
		if (CarriedCharacter)
		{
			Server_DropCharacter();
		}
		if (CarrierCharacter)
		{
			CarrierCharacter->Server_DropCharacter();
		}
	}
	Super::Destroyed();
}

FName AP48PlayerCharacter::GetRagdollRootBoneName() const
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (MeshComp->DoesSocketExist(TEXT("hips")))
		{
			return TEXT("hips");
		}
		if (MeshComp->DoesSocketExist(TEXT("pelvis")))
		{
			return TEXT("pelvis");
		}
	}
	return TEXT("hips");
}

void AP48PlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsInThrownRagdoll)
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			const FName RootBone = GetRagdollRootBoneName();
			const FVector RootLoc = MeshComp->GetSocketLocation(RootBone);
			SetActorLocation(RootLoc, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

void AP48PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->bEnablePhysicsOnDedicatedServer = true;
		MeshComp->SetPhysicsBlendWeight(0.5f);
		
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		
		MeshComp->SetAllBodiesBelowSimulatePhysics(TEXT("spine"), true, true);
	}
	
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
	
	InitializeStatsFromDataTable();
	
	//콜리전(기본값: 끄기)
	if (RightHandHitbox)
	{
		RightHandHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RightHandHitbox->OnComponentBeginOverlap.AddDynamic(this, &AP48PlayerCharacter::OnRightHandOverlap);
	}
	
	if (AP48PlayerState* PS = GetPlayerState<AP48PlayerState>())
	{
		PS->SetAlive(true);
		PS->ResetStunCount();
		PS->ResetHasWeapon();
	}
	
	if (Controller != nullptr)
	{
		FRotator StartRot = Controller->GetControlRotation();
		StartRot.Pitch = -25.0f;
		Controller->SetControlRotation(StartRot);
	}
	
	if (NicknameWidgetComponent)
	{
		NicknameWidgetComponent->UpdateNickname();
	}
}

void AP48PlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
	
	InitializeStatsFromDataTable();
	
	if (AbilitySystemComponent && GroggyAttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			GroggyAttributeSet->GetGroggyAttribute()
			).AddUObject(this, &AP48PlayerCharacter::OnGroggyChanged);
		
		const FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("State.Stunned"));
		AbilitySystemComponent->RegisterGameplayTagEvent(StunTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &AP48PlayerCharacter::OnStunTagChanged);
	}
	
	if (AP48PlayerState* PS = GetPlayerState<AP48PlayerState>())
	{
		PS->SetAlive(true);
		PS->ResetStunCount();
		PS->ResetHasWeapon();
	}
}

void AP48PlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AP48PlayerCharacter, Weapon);
	DOREPLIFETIME(AP48PlayerCharacter, CarriedCharacter);
	DOREPLIFETIME(AP48PlayerCharacter, CarrierCharacter);
}

void AP48PlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
	
	InitializeStatsFromDataTable();
	
	if (NicknameWidgetComponent)
	{
		NicknameWidgetComponent->UpdateNickname();
	}
	
	const FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("State.Stunned"));
	AbilitySystemComponent->RegisterGameplayTagEvent(StunTag, EGameplayTagEventType::NewOrRemoved)
	.AddUObject(this, &AP48PlayerCharacter::OnStunTagChanged);
	
	if (AP48PlayerState* PS = GetPlayerState<AP48PlayerState>())
	{
		PS->SetAlive(true);
		PS->ResetStunCount();
		PS->ResetHasWeapon();
	}
	
	if (AbilitySystemComponent && GroggyAttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			GroggyAttributeSet->GetGroggyAttribute()).AddUObject(this, &AP48PlayerCharacter::OnGroggyChanged);
	}
}


void AP48PlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Move)
		{
			EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AP48PlayerCharacter::Move);
		}
		
		if (IA_Look)
		{
			EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AP48PlayerCharacter::Look);
		}
		
		if (IA_Jump)
		{
			EIC->BindAction(IA_Jump, ETriggerEvent::Triggered, this, &AP48PlayerCharacter::Jump);
			EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AP48PlayerCharacter::StopJumping);
		}
		
		if (IA_Run)
		{
			EIC->BindAction(IA_Run, ETriggerEvent::Started, this, &AP48PlayerCharacter::Run);
			EIC->BindAction(IA_Run, ETriggerEvent::Completed, this, &AP48PlayerCharacter::StopRun);
		}
		
		if (IA_Attack)
		{
			EIC->BindAction(IA_Attack, ETriggerEvent::Started, this, &AP48PlayerCharacter::AttackHandle);
		}
		if (IA_PickUp)
		{
			EIC->BindAction(IA_PickUp, ETriggerEvent::Started, this, &AP48PlayerCharacter::EquipWeaponHandle);
		}
	}
}

void AP48PlayerCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	
	MovementVector = MovementVector.GetClampedToMaxSize(1.0f);
	
	if (Controller != nullptr)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation = FRotator(0.f, Rotation.Yaw, 0.f);
		
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		
		AddMovementInput(ForwardDirection, MovementVector.X);
		AddMovementInput(RightDirection, MovementVector.Y);
	}
}

void AP48PlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookValue = Value.Get<FVector2D>();
	
	if (Controller != nullptr)
	{
		AddControllerYawInput(LookValue.X);
	}
}

void AP48PlayerCharacter::Run()
{
	if (bInputBlocked)
	{
		return;
	}
	
	if (!AbilitySystemComponent || !RunEffectClass)
	{
		return;	
	}
	
	if (RunEffectHandle.IsValid())
	{
		return;
	}
	
	FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
	ContextHandle.AddSourceObject(this);
	
	RunEffectHandle = AbilitySystemComponent->BP_ApplyGameplayEffectToSelf(
		RunEffectClass,
		1.0f,
		ContextHandle
		);
	
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = CurrentStatRow.WalkSpeed * CurrentStatRow.RunSpeedMultiplier;
	}
	
	if (!HasAuthority())
	{
		Server_SetMaxWalkSpeed(CurrentStatRow.WalkSpeed * CurrentStatRow.RunSpeedMultiplier);
	}
	
}

void AP48PlayerCharacter::StopRun()
{
	if (!AbilitySystemComponent || !RunEffectClass)
	{
		return;
	}
	
	if (RunEffectHandle.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(RunEffectHandle);
		RunEffectHandle.Invalidate();
	}
	
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = CurrentStatRow.WalkSpeed;
	}
	
	if (!HasAuthority())
	{
		Server_SetMaxWalkSpeed(CurrentStatRow.WalkSpeed);
	}
}

void AP48PlayerCharacter::Jump()
{
	if (bInputBlocked)
	{
		return;
	}
	
	Super::Jump();
}

void AP48PlayerCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	if (JumpSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, JumpSound, GetActorLocation());
	}
}

void AP48PlayerCharacter::Attack()
{
	UAnimMontage* MontageToPlay = bEquipWeapon ? WeaponAttackMontage : PunchAttackMontage;
	
	if (MontageToPlay)
	{
		PlayAnimMontage(MontageToPlay);
	}

	// 공격자는 입력 즉시 재생하고, 원격 캐릭터는 몽타주 멀티캐스트에서 재생한다.
	if (bEquipWeapon && Weapon)
	{
		Weapon->PlaySwingSound();
	}
}

void AP48PlayerCharacter::AttackHandle()
{
	if (bInputBlocked)
	{
		return;
	}
	
	// 캐릭터를 들고 있는 상태에서는 공격 불가
	if (IsCarryingCharacter())
	{
		return;
	}
	
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		UAnimMontage* CurrentMontage = bEquipWeapon ? WeaponAttackMontage : PunchAttackMontage;
		
		if (CurrentMontage && AnimInstance->Montage_IsPlaying(CurrentMontage))
		{
			return;
		}
		
		Attack();
	}

	Server_Attack();
}

UAbilitySystemComponent* AP48PlayerCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AP48PlayerCharacter::InitializeStatsFromDataTable()
{
	if (!CharacterStatTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerCharacter] CharacterStatTable is none."));
		return;
	}
	
	static const FString ContextString(TEXT("Character Stat Context"));
	FCharacterStatRow* StatRow = CharacterStatTable->FindRow<FCharacterStatRow>(CharacterStatRowName, ContextString);
	
	if (StatRow)
	{
		CurrentStatRow = *StatRow;
		
		if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
		{
			MovementComp->MaxWalkSpeed = CurrentStatRow.WalkSpeed;
		}
		
		if (HasAuthority() && GroggyAttributeSet)
		{
			GroggyAttributeSet->InitMaxGroggy(CurrentStatRow.MaxGroggy);
		}
	}
}

void AP48PlayerCharacter::Server_SetMaxWalkSpeed_Implementation(float NewSpeed)
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = NewSpeed;
	}
}

void AP48PlayerCharacter::StartPunchAttack()
{
	HitActorThisPunch.Empty();
	if (RightHandHitbox)
	{
		RightHandHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void AP48PlayerCharacter::StopPunchAttack()
{
	if (RightHandHitbox)
	{
		RightHandHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	HitActorThisPunch.Empty();
}

void AP48PlayerCharacter::OnRightHandOverlap(
	UPrimitiveComponent* OverlappedComponent, 
	AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, 
	int32 OtherBodyIndex, 
	bool bFromSweep, 
	const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || OtherActor == CarriedCharacter)
	{
		return;
	}
	
	if (!IsLocallyControlled())
	{
		return;
	}
	
	AP48PlayerCharacter* OtherCharacter = Cast<AP48PlayerCharacter>(OtherActor);
	if (!OtherCharacter)
	{
		return;
	}
	
	if (OtherComp != OtherCharacter->GetCapsuleComponent())
	{
		return;
	}
	
	if (HitActorThisPunch.Contains(OtherActor))
	{
		return;
	}
	
	HitActorThisPunch.Add(OtherActor);
	
	if (RightHandHitbox)
	{
		RightHandHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	
	const FVector HitDir = (OtherActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	const FVector HitLoc = RightHandHitbox ? RightHandHitbox->GetComponentLocation() : OtherActor->GetActorLocation();
	
	Server_ApplyHit(OtherActor, HitLoc, HitDir);
}

void AP48PlayerCharacter::OnHit(const FVector& HitLocation, const FVector& HitDirection, float ImpulseStrength)
{
	const FVector Impulse = HitDirection.GetSafeNormal() * ImpulseStrength;
	
	LastHitDirection = HitDirection;
	
	if (AP48PlayerState* PS = GetPlayerState<AP48PlayerState>())
	{
		if (PS->IsAlive() && HasAuthority())
		{
			if (CarriedCharacter)
			{
				if (bEnableCarryDebug && GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Red, TEXT("[Carry Debug] 피격당해 들고 있던 캐릭터를 놓쳤습니다!"));
				}
				Server_DropCharacter();
			}
			Multicast_OnHit(HitLocation, Impulse);
		}
	}
}

void AP48PlayerCharacter::Multicast_OnHit_Implementation(const FVector& HitLocation, const FVector& Impulse)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}
	
	//MeshComp->AddImpulseAtLocation(Impulse, HitLocation);
	
	MeshComp->AddImpulse(Impulse, TEXT("chest"), true);
}

void AP48PlayerCharacter::Server_Attack_Implementation()
{
	if (bEquipWeapon)
	{
		if (Weapon)
		{
			Weapon->ResetAttackState();
		}
		Multicast_PlayWeaponMontage();
	}
	else
	{
		Multicast_PlayPunchMontage();
	}
}

void AP48PlayerCharacter::Multicast_PlayPunchMontage_Implementation()
{
	if (IsLocallyControlled())
	{
		return;
	}
	if (PunchAttackMontage)
	{
		PlayAnimMontage(PunchAttackMontage);
	}
}

void AP48PlayerCharacter::Multicast_PlayWeaponMontage_Implementation()
{
	if (IsLocallyControlled())
	{
		return;
	}
	if (WeaponAttackMontage)
	{
		PlayAnimMontage(WeaponAttackMontage);
	}
	if (Weapon)
	{
		Weapon->PlaySwingSound();
	}
}

void AP48PlayerCharacter::ApplyGroggyDamage(AActor* HitActor)
{
	if (!HasAuthority() || !IsValid(HitActor))
	{
		return;
	}
	
	if (!GroggyEffectClass)
	{
		return;
	}
	
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!TargetASC)
	{
		return;
	}
	
	FGameplayEffectContextHandle ContextHandle = TargetASC->MakeEffectContext();
	ContextHandle.AddSourceObject(this);
	
	FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(GroggyEffectClass, 1.0f, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		return;
	}
	
	const FGameplayTag GroggyDataTag = FGameplayTag::RequestGameplayTag(FName("Data.GroggyDamage"));
	SpecHandle.Data->SetSetByCallerMagnitude(GroggyDataTag, GroggyDamage);
	
	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

bool AP48PlayerCharacter::Server_ApplyHit_Validate(AActor* HitActor, const FVector& HitLoc, const FVector& HitDir)
{
	return IsValid(HitActor);	
}

void AP48PlayerCharacter::Server_ApplyHit_Implementation(AActor* HitActor, const FVector& HitLoc, const FVector& HitDir)
{
	ApplyGroggyDamage(HitActor);
	
	if (AP48PlayerCharacter* TargetCharacter = Cast<AP48PlayerCharacter>(HitActor))
	{
		TargetCharacter->OnHit(HitLoc, HitDir, 60000.f);
	}
}

void AP48PlayerCharacter::Multicast_PlayStunMontage_Implementation(bool bPlay, FRotator TargetRotation)
{
	if (!StunMontage)
	{
		return;
	}
	
	if (bPlay)
	{	
		if (!TargetRotation.IsZero())
		{
			SetActorRotation(FRotator(0.0f, TargetRotation.Yaw, 0.0f));
		}
		
		PlayAnimMontage(StunMontage);
	}
	else
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.2f, StunMontage);
		}
	}
}

void AP48PlayerCharacter::OnGroggyChanged(const struct FOnAttributeChangeData& Data)
{
	if (!AbilitySystemComponent || !GroggyAttributeSet)
	{
		return;
	}
	
	const float CurrentGroggy = Data.NewValue;
	const float MaxGroggy = GroggyAttributeSet->GetMaxGroggy();
	
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("1번 통과"));
		if (IsValid(NicknameWidgetComponent) == false)
		{
			UE_LOG(LogTemp, Warning, TEXT("2번 생성 실패"));
			return;
		}
        
		UP48PlayerNameWidget* NicknameWidget = Cast<UP48PlayerNameWidget>(NicknameWidgetComponent->GetUserWidgetObject());
		if (IsValid(NicknameWidget) == false)
		{
			UE_LOG(LogTemp, Warning, TEXT("3번 생성 실패!"));
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("그로기 동기화됨!"));
		NicknameWidget->UpdateGroggy(CurrentGroggy, MaxGroggy);
        
		return;
	}
	
	if (CurrentGroggy >= MaxGroggy && MaxGroggy > 0.0f)
	{
		const FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("State.Stunned"));
		
		if (!AbilitySystemComponent->HasMatchingGameplayTag(StunTag))
		{
			FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
			ContextHandle.AddSourceObject(this);
			
			if (ResetGroggyEffectClass)
			{
				AbilitySystemComponent->BP_ApplyGameplayEffectToSelf(ResetGroggyEffectClass, 1.0f, ContextHandle);
			}
			else
			{
				GroggyAttributeSet->SetGroggy(0.0f);
			}
			
			if (StunEffectClass)
			{
				AbilitySystemComponent->BP_ApplyGameplayEffectToSelf(StunEffectClass, 1.0f, ContextHandle);
			}
			
			FRotator FaceAttackerRot = GetActorRotation();
		}
	}
}

void AP48PlayerCharacter::OnStunTagChanged(const struct FGameplayTag CallbackTag, int32 NewCount)
{
	const bool bIsStunned = (NewCount > 0);
	
	if (HasAuthority())
	{
		if (bIsStunned && CarriedCharacter)
		{
			if (bEnableCarryDebug && GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, TEXT("[Carry Debug] 스턴으로 인해 들고 있던 캐릭터를 놓쳤습니다!"));
			}
			Server_DropCharacter();
		}
		if (!bIsStunned && CarrierCharacter)
		{
			if (bEnableCarryDebug && GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan, TEXT("[Carry Debug] 스턴이 해제되었으나 계속 들려있는 상태를 유지합니다!"));
			}
		}

		FRotator LookAtRotation = FRotator::ZeroRotator;
		
		if (bIsStunned && !LastHitDirection.IsNearlyZero())
		{
			const FVector FaceToAttackDirection = -LastHitDirection;
			
			LookAtRotation = FRotator(0.0f, FaceToAttackDirection.Rotation().Yaw, 0.0f);
			
			if (AP48PlayerState* PS = GetPlayerState<AP48PlayerState>())
			{
				PS->AddStunCount();
				
				if (PS->GetStunCount() >= MaxStunCount)
				{
					UE_LOG(LogTemp, Error, TEXT("[%s] 사망"), *GetName());
					Death();
					return;
				}
			}
		}
		
		Multicast_PlayStunMontage(bIsStunned, LookAtRotation);
	}
	
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (bIsStunned)
		{
			MoveComp->DisableMovement();
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("MoveMode: Disable"));
			}
		}	
		else
		{
			// 현재 상대방에게 들려있는 상태라면 워킹 모드로 전환하지 않고 들린 상태 유지
			if (IsBeingCarried())
			{
				MoveComp->DisableMovement();
				if (USkeletalMeshComponent* MeshComp = GetMesh())
				{
					if (bEnableCarriedRagdoll)
					{
						MeshComp->SetPhysicsBlendWeight(CarriedPhysicsBlendWeight);
					}
					else
					{
						MeshComp->SetPhysicsBlendWeight(0.0f);
					}
				}
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("MoveMode: Disable (Being Carried)"));
				}
			}
			else
			{
				MoveComp->SetMovementMode(MOVE_Walking);
				
				if (USkeletalMeshComponent* MeshComp = GetMesh())
				{
					MeshComp->SetAllBodiesBelowSimulatePhysics(TEXT("spine"), true, true);
					MeshComp->SetPhysicsBlendWeight(0.5f);
				}
				
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("MoveMode: MOVE_Walking"));
				}
			}
		}
	}
}

void AP48PlayerCharacter::EquipWeaponHandle()
{
	if (bInputBlocked || !IsLocallyControlled())
	{
		return;
	}
	
	// 이미 캐릭터를 들고 있다면 내려놓기(Drop) 대신 던지기(Throw) 수행
	if (CarriedCharacter)
	{
		if (bEnableCarryDebug && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("[Carry Debug] 들고 있던 캐릭터를 던집니다."));
		}
		if (ThrowMontage)
		{
			PlayAnimMontage(ThrowMontage);
		}
		Server_ThrowCharacter();
		return;
	}

	AP48PlayerState* PS = GetPlayerState<AP48PlayerState>();
	if (!PS)
	{
		return;
	}
	
	if (PS->HasWeapon())
	{
		Server_DropWeapon();
		return;
	}
	
	const FVector Center = GetActorLocation();
	const float SearchRadius = 150.f;
	
	FCollisionObjectQueryParams ObjectParams(FCollisionObjectQueryParams::AllDynamicObjects);
	FCollisionShape Sphere = FCollisionShape::MakeSphere(SearchRadius);
	
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, Sphere);
	
	AP48WeaponBase* ClosestWeapon = nullptr;
	AP48PlayerCharacter* ClosestStunnedEnemy = nullptr;
	
	float MinWeaponDist = MAX_FLT;
	float MinEnemyDist = MAX_FLT;

	const FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("State.Stunned"));
	
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OverlapActor = Overlap.GetActor();
		if (!OverlapActor || OverlapActor == this)
		{
			continue;
		}

		if (AP48WeaponBase* FoundWeapon = Cast<AP48WeaponBase>(OverlapActor))
		{
			if (FoundWeapon->GetOwner() == nullptr)
			{
				const float Dist = FVector::DistSquared(Center, FoundWeapon->GetActorLocation());
				
				if (Dist < MinWeaponDist)
				{
					MinWeaponDist = Dist;
					ClosestWeapon = FoundWeapon;
				}
			}
		}
		else if (AP48PlayerCharacter* TargetChar = Cast<AP48PlayerCharacter>(OverlapActor))
		{
			if (TargetChar->IsAlive() && !TargetChar->IsBeingCarried())
			{
				if (UAbilitySystemComponent* TargetASC = TargetChar->GetAbilitySystemComponent())
				{
					if (TargetASC->HasMatchingGameplayTag(StunTag))
					{
						const float Dist = FVector::DistSquared(Center, TargetChar->GetActorLocation());
						if (Dist < MinEnemyDist)
						{
							MinEnemyDist = Dist;
							ClosestStunnedEnemy = TargetChar;
						}
					}
				}
			}
		}
	}
	
	if (bEnableCarryDebug)
	{
		const FColor SphereColor = (ClosestStunnedEnemy || ClosestWeapon) ? FColor::Green : FColor::Cyan;
		DrawDebugSphere(GetWorld(), Center, SearchRadius, 16, SphereColor, false, 1.5f, 0, 1.5f);
	}

	if (ClosestStunnedEnemy && (MinEnemyDist < MinWeaponDist || !ClosestWeapon))
	{
		if (bEnableCarryDebug && GEngine)
		{
			const FString Msg = FString::Printf(TEXT("[Carry Debug] 스턴 적 감지 성공: %s (거리: %.1f)"), *ClosestStunnedEnemy->GetName(), FMath::Sqrt(MinEnemyDist));
			GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Green, Msg);
		}
		UE_LOG(LogTemp, Warning, TEXT("[Client]: 스턴 적 발견: %s -> Server_PickUpCharacter 호출"), *ClosestStunnedEnemy->GetName());
		Server_PickUpCharacter(ClosestStunnedEnemy);
	}
	else if (ClosestWeapon)
	{
		if (bEnableCarryDebug && GEngine)
		{
			const FString Msg = FString::Printf(TEXT("[Carry Debug] 무기 감지 성공: %s (거리: %.1f)"), *ClosestWeapon->GetName(), FMath::Sqrt(MinWeaponDist));
			GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Cyan, Msg);
		}
		UE_LOG(LogTemp, Warning, TEXT("[Client]: 무기 발견: %s -> Server_EquipWeapon호출"), *ClosestWeapon->GetName());
		Server_EquipWeapon(ClosestWeapon);
	}
	else
	{
		if (bEnableCarryDebug && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("[Carry Debug] 탐색 반경(150) 내에 무기나 스턴 적이 없습니다."));
		}
	}
}

void AP48PlayerCharacter::Server_EquipWeapon_Implementation(AP48WeaponBase* NewWeapon)
{
	if (!HasAuthority() || !NewWeapon)
	{
		return;
	}
	
	if (NewWeapon->GetOwner())
	{
		UE_LOG(LogTemp, Warning, TEXT("다른 사람 소유 무기"));
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Server_EquipWeapon"));
	
	Weapon = NewWeapon;
	Weapon->SetOwner(this);
	
	OnRep_Weapon();
	
	if (AP48PlayerState* PS = GetPlayerState<AP48PlayerState>())
	{
		PS->SetHasWeapon(true);
	}
	
	
}

void AP48PlayerCharacter::Server_DropWeapon_Implementation()
{
	if (!HasAuthority() || !Weapon)
	{
		return;
	}
	
	AP48WeaponBase* DroppingWeapon = Weapon;
	
	Weapon = nullptr;
	OnRep_Weapon();
	
	if (AP48PlayerState* PS = GetPlayerState<AP48PlayerState>())
	{
		PS->SetHasWeapon(false);
	}
	
	const FVector ThrowImpulse = (GetActorForwardVector() + FVector(0.f, 0.f, 0.3f)).GetSafeNormal() * 400.f;
	DroppingWeapon->OnDropped(ThrowImpulse);
}

void AP48PlayerCharacter::OnRep_Weapon()
{
	if (Weapon)
	{
		bEquipWeapon = true;
		Weapon->SetOwner(this);
		
		WeaponAttackMontage = Weapon->GetWeaponData().SwingAnim.LoadSynchronous();
		
		UStaticMeshComponent* WeaponMesh = Weapon->FindComponentByClass<UStaticMeshComponent>();
		if (!WeaponMesh)
		{
			UE_LOG(LogTemp, Error, TEXT("Can't find staticMesh"));
			return;
		}
		
		WeaponMesh->SetSimulatePhysics(false);
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		
		const FVector WeaponScale = Weapon->GetActorScale3D();
		
		const FAttachmentTransformRules AttachRules(
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::KeepWorld,
			false
			);
		
		Weapon->AttachToComponent(GetMesh(), AttachRules, TEXT("weaponslot_r"));
		Weapon->SetActorScale3D(WeaponScale);
		
		const FWeaponDataRow& Data = Weapon->GetWeaponData();
		WeaponMesh->SetRelativeLocationAndRotation(Data.RelativeLocation, Data.RelativeRotator);
		
		UE_LOG(LogTemp, Warning, TEXT("[Client] 무기 소켓 장착 성공: %s"), *Weapon->GetName());
	}
	else
	{
		bEquipWeapon = false;
		WeaponAttackMontage = nullptr;
	}
}

void AP48PlayerCharacter::SetInputBlocked(bool bBlocked)
{
	bInputBlocked = bBlocked;
	if (bBlocked)
	{
		StopRun();
	}
}

void AP48PlayerCharacter::Death()
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (CarriedCharacter)
	{
		Server_DropCharacter();
	}
	if (CarrierCharacter)
	{
		CarrierCharacter->Server_DropCharacter();
	}

	GetWorld()->GetTimerManager().ClearTimer(RagdollRecoverTimerHandle);
	bIsInThrownRagdoll = false;
	SetActorTickEnabled(false);

	AP48PlayerState* PS = GetPlayerState<AP48PlayerState>();
	
	if (!PS)
	{
		return;
	}
	
	Multicast_DeathRagDoll();
	
	PS->OnDeath();
}

void AP48PlayerCharacter::Multicast_DeathRagDoll_Implementation()
{
	bIsInThrownRagdoll = false;
	SetActorTickEnabled(false);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}
	
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CapsuleComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	}
	
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->StopAllMontages(0.0f);
		}
		
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComp->SetAllBodiesSimulatePhysics(true);
		MeshComp->SetSimulatePhysics(true);
		MeshComp->WakeAllRigidBodies();
		MeshComp->bBlendPhysics = false;
		MeshComp->SetPhysicsBlendWeight(1.0f);
	}
	
	if (NicknameWidgetComponent)
	{
		NicknameWidgetComponent->SetVisibility(false);
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[%s] Multi_DeathRagDoll: 순수 래그돌 연출 완료 "), *GetName());
}

bool AP48PlayerCharacter::IsAlive() const
{
	if (const AP48PlayerState* PS = GetPlayerState<AP48PlayerState>())
	{
		return PS->IsAlive();
	}
	return false;
}

void AP48PlayerCharacter::Server_PickUpCharacter_Implementation(AP48PlayerCharacter* TargetCharacter)
{
	if (!HasAuthority() || !TargetCharacter || TargetCharacter == this)
	{
		return;
	}

	AP48PlayerState* PS = GetPlayerState<AP48PlayerState>();
	if (!PS || PS->HasWeapon() || bEquipWeapon || CarriedCharacter)
	{
		return;
	}

	if (!TargetCharacter->IsAlive() || TargetCharacter->IsBeingCarried() || TargetCharacter->bIsInThrownRagdoll)
	{
		return;
	}

	const FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("State.Stunned"));
	UAbilitySystemComponent* TargetASC = TargetCharacter->GetAbilitySystemComponent();
	if (!TargetASC || !TargetASC->HasMatchingGameplayTag(StunTag))
	{
		return;
	}

	// 타겟이 무기를 들고 있었다면 떨어뜨리도록 처리
	if (TargetCharacter->Weapon)
	{
		TargetCharacter->Server_DropWeapon();
	}

	CarriedCharacter = TargetCharacter;
	TargetCharacter->CarrierCharacter = this;

	OnRep_CarriedCharacter();
	TargetCharacter->OnRep_CarrierCharacter();

	TargetCharacter->OnPickedUpBy(this);

	if (bEnableCarryDebug && GEngine)
	{
		const FString Msg = FString::Printf(TEXT("[Server]: %s -> %s 오른손(handslot_r) 들기 성공"), *GetName(), *TargetCharacter->GetName());
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Emerald, Msg);
	}
	UE_LOG(LogTemp, Warning, TEXT("[Server] %s가 %s를 오른손에 들었습니다."), *GetName(), *TargetCharacter->GetName());
}

void AP48PlayerCharacter::Server_ThrowCharacter_Implementation()
{
	if (!HasAuthority() || !CarriedCharacter)
	{
		return;
	}

	AP48PlayerCharacter* ThrowTarget = CarriedCharacter;
	CarriedCharacter = nullptr;
	ThrowTarget->CarrierCharacter = nullptr;

	OnRep_CarriedCharacter();

	const float ActualForwardSpeed = ThrowForwardSpeed > 0.0f ? ThrowForwardSpeed : 1800.0f;
	const float ActualUpSpeed = ThrowUpSpeed > 0.0f ? ThrowUpSpeed : 600.0f;

	const FVector Forward = GetActorForwardVector();
	const FVector Up = FVector::UpVector;
	const FVector ThrowVelocity = (Forward * ActualForwardSpeed) + (Up * ActualUpSpeed);

	// 던져지는 타겟이 캐리어의 캡슐이나 바닥과 겹쳐서 속도가 0으로 초기화되는 현상 방지:
	// 캐리어 전방 100cm, 지면 위 40cm 여유 공간의 안전한 공중 릴리즈 위치 계산
	const FVector SafeReleaseLocation = GetActorLocation() + (Forward * 100.0f) + FVector(0.f, 0.f, 40.0f);
	const FRotator SafeReleaseRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f);

	if (bEnableCarryDebug)
	{
		DrawDebugDirectionalArrow(GetWorld(), SafeReleaseLocation, SafeReleaseLocation + ThrowVelocity.GetSafeNormal() * 200.f, 40.0f, FColor::Red, false, 2.5f, 0, 3.0f);
		if (GEngine)
		{
			const FString Msg = FString::Printf(TEXT("[Server]: %s -> %s 래그돌 던지기 발동 (속도: %.1f)"), *GetName(), *ThrowTarget->GetName(), ThrowVelocity.Size());
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, Msg);
		}
	}

	// 1. 모든 클라이언트에 풀 래그돌 상태로 던지기 실행
	ThrowTarget->Multicast_OnThrown(SafeReleaseLocation, SafeReleaseRotation, ThrowVelocity, this);

	Multicast_PlayThrowMontage();

	// 2. 약 1~2초 동안 날아가고 바닥에 구른 뒤 다시 일어나도록 서버 타이머 등록
	const float RecoverTime = RagdollRecoverDuration > 0.0f ? RagdollRecoverDuration : 1.5f;
	GetWorld()->GetTimerManager().ClearTimer(ThrowTarget->RagdollRecoverTimerHandle);

	TWeakObjectPtr<AP48PlayerCharacter> WeakTarget = ThrowTarget;
	GetWorld()->GetTimerManager().SetTimer(ThrowTarget->RagdollRecoverTimerHandle, [WeakTarget]()
	{
		if (WeakTarget.IsValid() && WeakTarget->HasAuthority())
		{
			WeakTarget->Server_RecoverFromRagdoll();
		}
	}, RecoverTime, false);

	UE_LOG(LogTemp, Warning, TEXT("[Server] %s가 %s를 래그돌로 던졌습니다. 속도: %s"), *GetName(), *ThrowTarget->GetName(), *ThrowVelocity.ToString());
}

void AP48PlayerCharacter::Server_DropCharacter_Implementation()
{
	if (!HasAuthority() || !CarriedCharacter)
	{
		return;
	}

	AP48PlayerCharacter* DroppedTarget = CarriedCharacter;
	CarriedCharacter = nullptr;
	DroppedTarget->CarrierCharacter = nullptr;

	OnRep_CarriedCharacter();

	const FVector Forward = GetActorForwardVector();
	const FVector SafeDropLocation = GetActorLocation() + (Forward * 75.0f);
	const FRotator SafeDropRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f);

	if (bEnableCarryDebug && GEngine)
	{
		const FString Msg = FString::Printf(TEXT("[Server]: %s -> %s 내려놓기"), *GetName(), *DroppedTarget->GetName());
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, Msg);
	}

	DroppedTarget->Multicast_OnDropped(SafeDropLocation, SafeDropRotation, this);

	UE_LOG(LogTemp, Warning, TEXT("[Server] %s가 %s를 내려놓았습니다."), *GetName(), *DroppedTarget->GetName());
}

void AP48PlayerCharacter::OnRep_CarriedCharacter()
{
}

void AP48PlayerCharacter::OnRep_CarrierCharacter()
{
	if (CarrierCharacter)
	{
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
			MoveComp->DisableMovement();
		}

		if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
		{
			CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			MeshComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);

			if (bEnableCarriedRagdoll)
			{
				MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

				// 루트 본은 최상위 본이므로 시뮬레이션 시 캡슐과 이탈해 덜덜 떨리는 현상 방지.
				// spine(상체)과 다리를 시뮬레이션하여 루트는 손에 고정된 채 자연스럽게 흔들리도록 함.
				const FName RootBone = GetRagdollRootBoneName();
				MeshComp->SetAllBodiesBelowSimulatePhysics(RootBone, false, false);
				if (FBodyInstance* RootBody = MeshComp->GetBodyInstance(RootBone))
				{
					RootBody->SetInstanceSimulatePhysics(false);
				}

				MeshComp->SetAllBodiesBelowSimulatePhysics(CarriedPhysicsBoneName, true, true);

				if (MeshComp->GetPhysicsAsset())
				{
					if (MeshComp->GetBodyInstance(TEXT("upperleg_l")))
					{
						MeshComp->SetAllBodiesBelowSimulatePhysics(TEXT("upperleg_l"), true, true);
					}
					else if (MeshComp->GetBodyInstance(TEXT("thigh_l")))
					{
						MeshComp->SetAllBodiesBelowSimulatePhysics(TEXT("thigh_l"), true, true);
					}

					if (MeshComp->GetBodyInstance(TEXT("upperleg_r")))
					{
						MeshComp->SetAllBodiesBelowSimulatePhysics(TEXT("upperleg_r"), true, true);
					}
					else if (MeshComp->GetBodyInstance(TEXT("thigh_r")))
					{
						MeshComp->SetAllBodiesBelowSimulatePhysics(TEXT("thigh_r"), true, true);
					}
				}

				MeshComp->bBlendPhysics = true;
				MeshComp->SetPhysicsBlendWeight(CarriedPhysicsBlendWeight);
				MeshComp->WakeAllRigidBodies();
			}
			else
			{
				MeshComp->SetPhysicsBlendWeight(0.0f);
			}
		}

		FName SocketToUse = CarrySocketName;
		if (CarrierCharacter->GetMesh())
		{
			if (!CarrierCharacter->GetMesh()->DoesSocketExist(SocketToUse))
			{
				if (CarrierCharacter->GetMesh()->DoesSocketExist(TEXT("weaponslot_r")))
				{
					SocketToUse = TEXT("weaponslot_r");
				}
				else if (CarrierCharacter->GetMesh()->DoesSocketExist(TEXT("hand_r")))
				{
					SocketToUse = TEXT("hand_r");
				}
			}
		}

		// 1. 소켓의 현재 월드 위치 가져오기
		const FVector SocketWorldLocation = CarrierCharacter->GetMesh()->GetSocketLocation(SocketToUse);
		
		// 2. 캐리어 방향 기준으로 직관적인 월드 위치 계산 (전방/우측/높이 오프셋)
		const FVector CarrierForward = CarrierCharacter->GetActorForwardVector();
		const FVector CarrierRight = CarrierCharacter->GetActorRightVector();
		const FVector TargetWorldLocation = SocketWorldLocation 
			+ (CarrierForward * CarriedForwardOffset) 
			+ (CarrierRight * CarriedRightOffset) 
			+ FVector(0.f, 0.f, CarriedHeightOffset);

		// 3. 캐리어의 Yaw 회전각에 맞추어 똑바로 선(Upright: Pitch=0, Roll=0) 월드 회전 계산
		const FRotator TargetWorldRotation = FRotator(0.0f, CarrierCharacter->GetActorRotation().Yaw + CarriedYawOffset, 0.0f);

		// 4. 먼저 올바른 위치와 회전으로 이동시킨 뒤, 월드 트랜스폼을 유지(KeepWorldTransform)하며 소켓에 부착
		SetActorLocationAndRotation(TargetWorldLocation, TargetWorldRotation);
		AttachToComponent(CarrierCharacter->GetMesh(), FAttachmentTransformRules::KeepWorldTransform, SocketToUse);

		if (bEnableCarryDebug && GEngine)
		{
			const FString Msg = FString::Printf(TEXT("[%s] %s의 [%s] 소켓에 부착 완료! (Ragdoll: %.2f)"), 
				*GetName(), *CarrierCharacter->GetName(), *SocketToUse.ToString(), bEnableCarriedRagdoll ? CarriedPhysicsBlendWeight : 0.0f);
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, Msg);
			DrawDebugCoordinateSystem(GetWorld(), TargetWorldLocation, TargetWorldRotation, 40.0f, false, 3.0f);
		}
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
		{
			CapsuleComp->SetCollisionProfileName(TEXT("Pawn"));
			CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}

		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			MeshComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);

			const FName RootBone = GetRagdollRootBoneName();
			MeshComp->SetAllBodiesBelowSimulatePhysics(RootBone, false, false);
			MeshComp->SetAllBodiesBelowSimulatePhysics(CarriedPhysicsBoneName, true, true);
			MeshComp->bBlendPhysics = true;
			MeshComp->SetPhysicsBlendWeight(0.5f);
		}

		if (bEnableCarryDebug && GEngine)
		{
			const FString Msg = FString::Printf(TEXT("[%s] Carrier로부터 분리(Detach) 완료"), *GetName());
			GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Orange, Msg);
		}
	}
}

void AP48PlayerCharacter::OnPickedUpBy(AP48PlayerCharacter* InCarrier)
{
	OnRep_CarrierCharacter();
}

void AP48PlayerCharacter::OnThrown(const FVector& ReleaseLocation, const FRotator& ReleaseRotation, const FVector& ThrowVelocity, AP48PlayerCharacter* InCarrier)
{
	Multicast_OnThrown(ReleaseLocation, ReleaseRotation, ThrowVelocity, InCarrier);
}

void AP48PlayerCharacter::OnDroppedFromCarrier(const FVector& DropLocation, const FRotator& DropRotation, AP48PlayerCharacter* InCarrier)
{
	Multicast_OnDropped(DropLocation, DropRotation, InCarrier);
}

void AP48PlayerCharacter::Multicast_OnThrown_Implementation(const FVector& ReleaseLocation, const FRotator& ReleaseRotation, const FVector& ThrowVelocity, AP48PlayerCharacter* InCarrier)
{
	CarrierCharacter = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// 1. 발사 위치로 이동
	SetActorLocationAndRotation(ReleaseLocation, ReleaseRotation, false, nullptr, ETeleportType::TeleportPhysics);

	// 2. 네트워크 무브먼트 복제 일시 해제 (비행 중 캡슐이 메시를 제자리로 강제 롤백하지 못하도록 차단)
	SetReplicateMovement(false);

	// 3. 캡슐 콜리전 해제
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CapsuleComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	}

	// 4. 무브먼트 컴포넌트 완전 비활성화
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
		MoveComp->Deactivate();
	}

	// 5. 플레이어 입력 차단
	SetInputBlocked(true);

	// 6. 스켈레탈 메시를 캡슐로부터 완전히 분리하여 독립된 순수 물리 인형화
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->StopAllMontages(0.0f);
		}

		MeshComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// 던지는 플레이어(캐리어)나 다른 폰의 캡슐과 공중에서 부딪쳐 멈추지 않도록 비행 중에는 Pawn 채널 무시
		MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

		if (InCarrier)
		{
			MeshComp->IgnoreActorWhenMoving(InCarrier, true);
		}

		MeshComp->bEnablePhysicsOnDedicatedServer = true;

		// 모든 물리 바디(루트 본 hips 포함)를 강제로 시뮬레이션 가능하게 설정
		for (FBodyInstance* BodyInst : MeshComp->Bodies)
		{
			if (BodyInst)
			{
				BodyInst->SetInstanceSimulatePhysics(true);
			}
		}

		// 전신 물리 시뮬레이션 활성화
		MeshComp->SetAllBodiesSimulatePhysics(true);
		MeshComp->SetSimulatePhysics(true);
		MeshComp->bBlendPhysics = false;
		MeshComp->SetPhysicsBlendWeight(1.0f);

		MeshComp->WakeAllRigidBodies();

		// 파티 애니멀즈 스타일 물리 속도 및 공중 텀블링 회전각 부여
		MeshComp->SetAllPhysicsLinearVelocity(ThrowVelocity);
		const FName RootBone = GetRagdollRootBoneName();
		MeshComp->SetPhysicsAngularVelocityInDegrees(FVector(FMath::RandRange(-80.f, 80.f), 350.0f, FMath::RandRange(-50.f, 50.f)), false, RootBone);
	}

	// 7. 래그돌 비행 추적 활성화 (카메라가 날아가는 래그돌 골반을 실시간 추적)
	bIsInThrownRagdoll = true;
	SetActorTickEnabled(true);

	if (bEnableCarryDebug && GEngine)
	{
		const FString Msg = FString::Printf(TEXT("[%s] 파티 애니멀즈 풀 래그돌 비행 시작! (속도: %s)"), *GetName(), *ThrowVelocity.ToString());
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, Msg);
	}
}

void AP48PlayerCharacter::Server_RecoverFromRagdoll()
{
	if (!HasAuthority() || !IsAlive())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(RagdollRecoverTimerHandle);

	const FName RootBone = GetRagdollRootBoneName();
	FVector RootLoc = GetActorLocation();
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		RootLoc = MeshComp->GetSocketLocation(RootBone);
	}

	FHitResult GroundHit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (CarrierCharacter)
	{
		QueryParams.AddIgnoredActor(CarrierCharacter);
	}

	const FVector TraceStart = RootLoc + FVector(0.f, 0.f, 50.f);
	const FVector TraceEnd = RootLoc - FVector(0.f, 0.f, 500.f);

	float CapsuleHalfHeight = 65.0f;
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleHalfHeight = CapsuleComp->GetScaledCapsuleHalfHeight();
	}

	FVector StandLocation = RootLoc;
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
	{
		StandLocation = GroundHit.ImpactPoint + FVector(0.f, 0.f, CapsuleHalfHeight + 2.0f);
	}
	else
	{
		StandLocation = RootLoc + FVector(0.f, 0.f, CapsuleHalfHeight);
	}

	const FRotator StandRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f);

	Multicast_RecoverFromRagdoll(StandLocation, StandRotation);
}

void AP48PlayerCharacter::Multicast_RecoverFromRagdoll_Implementation(const FVector& StandLocation, const FRotator& StandRotation)
{
	bIsInThrownRagdoll = false;
	SetActorTickEnabled(false);

	SetActorLocationAndRotation(StandLocation, StandRotation, false, nullptr, ETeleportType::TeleportPhysics);

	// 1. 스켈레탈 메시 물리 시뮬레이션 해제 및 캡슐에 다시 장착
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetAllBodiesSimulatePhysics(false);
		for (FBodyInstance* BodyInst : MeshComp->Bodies)
		{
			if (BodyInst)
			{
				BodyInst->SetInstanceSimulatePhysics(false);
			}
		}

		MeshComp->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		MeshComp->SetRelativeLocation(FVector(0.f, 0.f, -65.f));
		MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		MeshComp->SetCollisionProfileName(TEXT("CharacterMesh"));
		MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

		const FName RootBone = GetRagdollRootBoneName();
		MeshComp->SetAllBodiesBelowSimulatePhysics(RootBone, false, false);
		MeshComp->SetAllBodiesBelowSimulatePhysics(CarriedPhysicsBoneName, true, true);
		MeshComp->bBlendPhysics = true;
		MeshComp->SetPhysicsBlendWeight(0.5f);
		MeshComp->WakeAllRigidBodies();
	}

	// 2. 캡슐 콜리전 정상 복원
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionProfileName(TEXT("Pawn"));
		CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CapsuleComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}

	// 3. 네트워크 이동 복제 복원
	SetReplicateMovement(true);

	// 4. 무브먼트 컴포넌트 재활성화 및 이동 모드/스턴 판정
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->Activate();
		MoveComp->Velocity = FVector::ZeroVector;

		const FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("State.Stunned"));
		if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(StunTag))
		{
			MoveComp->DisableMovement();
			if (StunMontage)
			{
				PlayAnimMontage(StunMontage);
			}
		}
		else
		{
			MoveComp->SetMovementMode(MOVE_Walking);
			SetInputBlocked(false);
		}
	}

	// 5. 기상 몽타주가 있다면 재생
	if (GetUpMontage)
	{
		PlayAnimMontage(GetUpMontage);
	}

	if (bEnableCarryDebug && GEngine)
	{
		const FString Msg = FString::Printf(TEXT("[%s] 래그돌에서 회복하여 다시 일어남!"), *GetName());
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Green, Msg);
	}
}

void AP48PlayerCharacter::Multicast_OnDropped_Implementation(const FVector& DropLocation, const FRotator& DropRotation, AP48PlayerCharacter* InCarrier)
{
	CarrierCharacter = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	SetActorLocationAndRotation(DropLocation, DropRotation, false, nullptr, ETeleportType::TeleportPhysics);

	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionProfileName(TEXT("Pawn"));
		CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		MeshComp->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);

		const FName RootBone = GetRagdollRootBoneName();
		MeshComp->SetAllBodiesBelowSimulatePhysics(RootBone, false, false);
		MeshComp->SetAllBodiesBelowSimulatePhysics(CarriedPhysicsBoneName, true, true);
		MeshComp->bBlendPhysics = true;
		MeshComp->SetPhysicsBlendWeight(0.5f);
		MeshComp->WakeAllRigidBodies();
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		const FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("State.Stunned"));
		if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(StunTag))
		{
			MoveComp->DisableMovement();
		}
		else
		{
			MoveComp->SetMovementMode(MOVE_Walking);
			SetInputBlocked(false);
		}
	}

	if (bEnableCarryDebug && GEngine)
	{
		const FString Msg = FString::Printf(TEXT("[%s] 바닥에 내려놓아짐"), *GetName());
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, Msg);
	}
}

void AP48PlayerCharacter::Multicast_PlayThrowMontage_Implementation()
{
	if (IsLocallyControlled())
	{
		return;
	}
	if (ThrowMontage)
	{
		PlayAnimMontage(ThrowMontage);
	}
}

void AP48PlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	const FGameplayTag StunTag = FGameplayTag::RequestGameplayTag(FName("State.Stunned"));
	if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(StunTag))
	{
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->DisableMovement();
		}
	}
}
