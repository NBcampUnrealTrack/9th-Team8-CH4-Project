#include "P48PlayerNameWidgetComponent.h"
#include "P48PlayerNameWidget.h"
//#include "HS/HSPlayerState.h"
#include "Project48/Character/P48PlayerCharacter.h"
#include "Project48/Character/P48PlayerState.h"

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

void UP48PlayerNameWidgetComponent::UpdateNickname()
{
	AP48PlayerCharacter* P48Character = Cast<AP48PlayerCharacter>(GetOwner());
	if (IsValid(P48Character) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("P48Character생성 못함"));
	}
	
	AP48PlayerState* PS = Cast<AP48PlayerState>(P48Character->GetPlayerState());
	if (IsValid(PS) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("PS생성 못함"));
		return;
	}
	
	UP48PlayerNameWidget* NicknameWidget = Cast<UP48PlayerNameWidget>(GetUserWidgetObject());
	if (IsValid(NicknameWidget) == false)
	{
		UE_LOG(LogTemp, Warning, TEXT("NW생성 못함"));
		return;
	}
	NicknameWidget->SetPlayerName(PS->GetNickname());
}
