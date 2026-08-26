#include "P48PlayerCharacter.h"

#include "Components/CapsuleComponent.h"

AP48PlayerCharacter::AP48PlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	GetCapsuleComponent()->InitCapsuleSize(34.f, 65.f);
	
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -65.f));
}

void AP48PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AP48PlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

