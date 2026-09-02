#include "P48PlayerController.h"

#include "P48PlayerState.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Project48/Game/P48GameModeBase.h"

void AP48PlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (IsLocalController() == false)
	{
		return;
	}
	
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			if (IsValid(DefaultMappingContext) == true)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
	
	if (IsValid(WidgetClass) == false)
	{
		return;
	}
	
	WidgetInstance = CreateWidget<UUserWidget>(this, WidgetClass);
	if (WidgetInstance)
	{
		WidgetInstance->AddToViewport();
		
		bShowMouseCursor = true;
		SetInputMode(FInputModeUIOnly());
	}
}

void AP48PlayerController::RequestReady()
{
	AP48PlayerState* PS = GetPlayerState<AP48PlayerState>();
	if (!PS)
	{
		return;
	}
	
	if (PS->IsReady())
	{
		return;
	}
	
	Server_SetReady(true);
}

bool AP48PlayerController::Server_SetReady_Validate(bool bNewReady)
{
	return GetPlayerState<AP48PlayerState>() != nullptr;
}

void AP48PlayerController::Server_SetReady_Implementation(bool bNewReady)
{
	AP48PlayerState* PS = GetPlayerState<AP48PlayerState>();
	
	if (!PS)
	{
		return;
	}
	
	if (PS->IsReady())
	{
		return;
	}
	
	PS->SetReady(bNewReady);
	
	if (AP48GameModeBase* GameMode = GetWorld()->GetAuthGameMode<AP48GameModeBase>())
	{
		GameMode->NotifyPlayerReadyStateChanged();
	}
}