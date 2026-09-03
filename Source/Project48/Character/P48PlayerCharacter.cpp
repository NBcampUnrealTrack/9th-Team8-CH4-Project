#include "P48PlayerCharacter.h"

#include "Project48/GAS/P48GroggyAttributeSet.h"
#include "Project48/DataTable/CharacterStatDataTypes.h"
#include "Project48/Effect/P48GE_Run.h"

#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SphereComponent.h"

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
	RightHandHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightHandHitbox->SetCollisionObjectType(ECC_WorldDynamic);
	RightHandHitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	RightHandHitbox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	
	RunEffectClass = UP48GE_Run::StaticClass();
}

void AP48PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetPhysicsBlendWeight(0.5f);
		
		MeshComp->SetAllBodiesBelowSimulatePhysics(TEXT("Chest"), true, true);
		
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
	
	InitializeStatsFromDataTable();
}

void AP48PlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
	
	InitializeStatsFromDataTable();
}

void AP48PlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
	
	InitializeStatsFromDataTable();
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
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Run"));
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
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Stop Run"));
	}
	
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
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		if (AnimInstance->Montage_IsPlaying(PunchAttackMontage) || AnimInstance->Montage_IsPlaying(WeaponAttackMontage))
		{
			return;
		}
		Attack();
	}
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
