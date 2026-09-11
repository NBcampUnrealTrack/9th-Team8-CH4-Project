#include "P48PlayerController.h"

#include "P48PlayerState.h"
#include "P48PlayerCharacter.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Project48/Game/P48GameModeBase.h"
#include "Project48/Maps/PCG/Common/P48PCGSeedState.h"
#include "EngineUtils.h"

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

void AP48PlayerController::ReportMapGenerationComplete(const int32 GenerationId)
{
	if (!IsLocalController()) { return; }
	if (HasAuthority()) { Server_ReportMapGenerationComplete_Implementation(GenerationId); }
	else { Server_ReportMapGenerationComplete(GenerationId); }
}

void AP48PlayerController::Server_ReportMapGenerationComplete_Implementation(const int32 GenerationId)
{
	for (TActorIterator<AP48PCGSeedState> It(GetWorld()); It; ++It)
	{
		It->NotifyClientGenerationComplete(this, GenerationId);
		break;
	}
}

void AP48PlayerController::Client_SetPlayInputBlocked_Implementation(bool bBlocked)
{
	SetIgnoreLookInput(bBlocked);
	SetIgnoreMoveInput(bBlocked);
	
	if (AP48PlayerCharacter* PlayerCharacter = GetPawn<AP48PlayerCharacter>())
	{
		PlayerCharacter->SetInputBlocked(bBlocked);
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[Client] Input Blocked State Changed: %s"), bBlocked ? TEXT("BLOCKED") : TEXT("UNBLOCKED"));
}
