// Fill out your copyright notice in the Description page of Project Settings.


#include "P48PlayerCharacter.h"


// Sets default values
AP48PlayerCharacter::AP48PlayerCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AP48PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AP48PlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AP48PlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

