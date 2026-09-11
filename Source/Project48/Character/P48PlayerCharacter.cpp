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
#include "Engine/OverlapResult.h"
#include "Net/UnrealNetwork.h"

AP48PlayerCharacter::AP48PlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->TargetArmLength = 400.0f;
	
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
}

void AP48PlayerCharacter::Destroyed()
{
	if (HasAuthority() && Weapon)
	{
		Weapon->Destroy();
		Weapon = nullptr;
	}
	Super::Destroyed();
}

void AP48PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetPhysicsBlendWeight(0.5f);
		
		MeshComp->SetAllBodiesBelowSimulatePhysics(TEXT("spine"), true, true);
		
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
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
		AddControllerPitchInput(LookValue.Y);
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
	
	
}

void AP48PlayerCharacter::AttackHandle()
{
	if (bInputBlocked)
	{
		return;
	}
	
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		if (AnimInstance->Montage_IsPlaying(PunchAttackMontage) || AnimInstance->Montage_IsPlaying(WeaponAttackMontage))
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
	if (!OtherActor || OtherActor == this)
	{
		return;
	}
	
	if (!IsLocallyControlled())
	{
		return;
	}
	
	if (OtherComp != Cast<AP48PlayerCharacter>(OtherActor)->GetCapsuleComponent())
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
	Multicast_PlayPunchMontage();
}

void AP48PlayerCharacter::Multicast_PlayPunchMontage_Implementation()
{
	if (PunchAttackMontage)
	{
		PlayAnimMontage(PunchAttackMontage);
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
					PS->OnDeath();
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

void AP48PlayerCharacter::EquipWeaponHandle()
{
	if (bInputBlocked)
	{
		return;
	}
	
	if (!IsLocallyControlled())
	{
		return;
	}
	
	AP48PlayerState* PS = GetPlayerState<AP48PlayerState>();
	if (!PS)
	{
		return;
	}
	
	if (PS->HasWeapon())
	{
		//TODO DropWeapon 예정
		return;
	}
	
	const FVector Center = GetActorLocation();
	const float SearchRadius = 150.f;
	
	FCollisionObjectQueryParams ObjectParams(FCollisionObjectQueryParams::AllDynamicObjects);
	FCollisionShape Sphere = FCollisionShape::MakeSphere(SearchRadius);
	
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, Sphere);
	
	AP48WeaponBase* ClosestWeapon = nullptr;
	
	float MinDist = MAX_FLT;
	
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (AP48WeaponBase* FoundWeapon = Cast<AP48WeaponBase>(Overlap.GetActor()))
		{
			if (FoundWeapon->GetOwner() == nullptr)
			{
				const float Dist = FVector::DistSquared(Center, FoundWeapon->GetActorLocation());
				
				if (Dist < MinDist)
				{
					MinDist = Dist;
					ClosestWeapon = FoundWeapon;
				}
			}
		}
	}
	
	if (ClosestWeapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Client]: 발견: %s -> Server_EquipWeapon호출"), *ClosestWeapon->GetName());
		Server_EquipWeapon(ClosestWeapon);
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
	
	OnRep_Weapon();
	
	if (AP48PlayerState* PS = GetPlayerState<AP48PlayerState>())
	{
		PS->SetHasWeapon(true);
	}
	
	
}

void AP48PlayerCharacter::OnRep_Weapon()
{
	if (Weapon)
	{
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
		//TODO: 나중에 무기 버릴 때 
	}
}

void AP48PlayerCharacter::SetInputBlocked(bool bBlocked)
{
	if (bBlocked)
	{
		StopRun();
	}
}

void AP48PlayerCharacter::Death()
{
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