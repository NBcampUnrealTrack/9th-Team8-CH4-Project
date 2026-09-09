#include "P48PlayerNameWidgetComponent.h"

UP48PlayerNameWidgetComponent::UP48PlayerNameWidgetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	//PrimaryComponentTick.TickInterval = 0.05f;
	
	SetWidgetSpace(EWidgetSpace::World);
}

void UP48PlayerNameWidgetComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (IsValid(PC) == false) return;
	
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector ToCamera = CamLoc - GetComponentLocation();
	const FRotator LookAtRot = ToCamera.Rotation();
		
	SetWorldRotation(LookAtRot);
}
