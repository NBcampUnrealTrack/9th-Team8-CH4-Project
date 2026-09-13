#include "P48SpectatorPawn.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/SpectatorPawnMovement.h"
#include "GameFramework/PlayerController.h"

AP48SpectatorPawn::AP48SpectatorPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
	
	if (USpectatorPawnMovement* MovementComp = Cast<USpectatorPawnMovement>(GetMovementComponent()))
	{
		MovementComp->MaxSpeed = 2000.f;
		MovementComp->Acceleration = 5000.f;
		MovementComp->Deceleration = 4000.f;
	}
}

void AP48SpectatorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	APlayerController* PC =Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	
	PC->SetIgnoreMoveInput(false);
	PC->SetIgnoreLookInput(false);
	
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->ClearAllMappings();
		
		if (SpectatorMappingContext)
		{
			Subsystem->AddMappingContext(SpectatorMappingContext, 0);
		}
		
		if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
		{
			if (IA_SpectatorMove)
			{
				EIC->BindAction(IA_SpectatorMove, ETriggerEvent::Triggered, this, &AP48SpectatorPawn::Move);
			}
			if (IA_SpectatorLook)
			{
				EIC->BindAction(IA_SpectatorLook, ETriggerEvent::Triggered, this, &AP48SpectatorPawn::Look);
			}
			if (IA_SpectatorAltitude)
			{
				EIC->BindAction(IA_SpectatorAltitude, ETriggerEvent::Triggered, this, &AP48SpectatorPawn::Altitude);
			}
		}
	}
}

void AP48SpectatorPawn::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	
	if (!Controller)
	{
		return;
	}
	
	const FVector2D NormalizedMovementVector = MovementVector.GetSafeNormal();
	
	const FRotator ControlRotation = Controller->GetControlRotation();
	const FVector ForwardDirection = FRotationMatrix(ControlRotation).GetUnitAxis(EAxis::X);
	
	const FRotator YawRotation(0.f, ControlRotation.Yaw, 0.f);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	
	AddMovementInput(ForwardDirection, NormalizedMovementVector.X);
	AddMovementInput(RightDirection, NormalizedMovementVector.Y);
}

void AP48SpectatorPawn::Look(const FInputActionValue& Value)
{
	const FVector2D LookValue = Value.Get<FVector2D>();
	AddControllerYawInput(LookValue.X);
	AddControllerPitchInput(LookValue.Y);
}

void AP48SpectatorPawn::Altitude(const FInputActionValue& Value)
{
	const float AxisValue = Value.Get<float>();
	
	AddMovementInput(FVector::UpVector, AxisValue);
}
