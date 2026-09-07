#include "P48LobbyPlayerController.h"

#include "P48LobbyGameMode.h"
#include "P48LobbyGameState.h"
#include "Project48/Character/P48PlayerState.h"
#include "Project48/Online/P48MatchmakingSubsystem.h"

void AP48LobbyPlayerController::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UP48MatchmakingSubsystem* MatchmakingSubsystem =
			GameInstance->GetSubsystem<UP48MatchmakingSubsystem>())
		{
			MatchmakingSubsystem->OnLeaveSessionCompleted.RemoveDynamic(
				this,
				&ThisClass::HandleLeaveSessionCompleted);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AP48LobbyPlayerController::RequestLobbyReady(bool bNewReady)
{
	if (IsLocalController())
	{
		Server_SetLobbyReady(bNewReady);
	}
}

void AP48LobbyPlayerController::RequestStartGame()
{
	if (IsLocalController())
	{
		Server_RequestStartGame();
	}
}

void AP48LobbyPlayerController::RequestLeaveLobby()
{
	if (IsLocalController() == false)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UP48MatchmakingSubsystem* MatchmakingSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UP48MatchmakingSubsystem>() : nullptr;
	if (IsValid(MatchmakingSubsystem) == false)
	{
		return;
	}

	MatchmakingSubsystem->OnLeaveSessionCompleted.RemoveDynamic(
		this,
		&ThisClass::HandleLeaveSessionCompleted);
	MatchmakingSubsystem->OnLeaveSessionCompleted.AddDynamic(
		this,
		&ThisClass::HandleLeaveSessionCompleted);

	if (MatchmakingSubsystem->LeaveSession() == false)
	{
		MatchmakingSubsystem->OnLeaveSessionCompleted.RemoveDynamic(
			this,
			&ThisClass::HandleLeaveSessionCompleted);
	}
}

bool AP48LobbyPlayerController::IsLobbyHost() const
{
	const AP48LobbyGameState* LobbyGameState =
		GetWorld() ? GetWorld()->GetGameState<AP48LobbyGameState>() : nullptr;
	return IsValid(LobbyGameState)
		&& LobbyGameState->IsHost(PlayerState);
}

bool AP48LobbyPlayerController::CanStartLobbyGame() const
{
	const AP48LobbyGameState* LobbyGameState =
		GetWorld() ? GetWorld()->GetGameState<AP48LobbyGameState>() : nullptr;
	return IsLobbyHost()
		&& IsValid(LobbyGameState)
		&& LobbyGameState->CanHostStartGame();
}

bool AP48LobbyPlayerController::IsLocalPlayerLobbyReady() const
{
	const AP48PlayerState* P48PlayerState =
		GetPlayerState<AP48PlayerState>();
	return IsValid(P48PlayerState)
		&& IsLobbyHost() == false
		&& P48PlayerState->IsReady();
}

void AP48LobbyPlayerController::Server_SetLobbyReady_Implementation(
	bool bNewReady)
{
	AP48LobbyGameMode* LobbyGameMode =
		GetWorld() ? GetWorld()->GetAuthGameMode<AP48LobbyGameMode>() : nullptr;
	if (IsValid(LobbyGameMode) == false
		|| LobbyGameMode->CanPlayerChangeReady(this) == false)
	{
		return;
	}

	AP48PlayerState* P48PlayerState =
		GetPlayerState<AP48PlayerState>();
	if (IsValid(P48PlayerState) == false)
	{
		return;
	}

	P48PlayerState->SetReady(bNewReady);
	LobbyGameMode->NotifyLobbyReadyStateChanged();
}

void AP48LobbyPlayerController::Server_RequestStartGame_Implementation()
{
	AP48LobbyGameMode* LobbyGameMode =
		GetWorld() ? GetWorld()->GetAuthGameMode<AP48LobbyGameMode>() : nullptr;
	if (IsValid(LobbyGameMode))
	{
		LobbyGameMode->RequestStartGame(this);
	}
}

void AP48LobbyPlayerController::HandleLeaveSessionCompleted(
	bool bWasSuccessful)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UP48MatchmakingSubsystem* MatchmakingSubsystem =
			GameInstance->GetSubsystem<UP48MatchmakingSubsystem>())
		{
			MatchmakingSubsystem->OnLeaveSessionCompleted.RemoveDynamic(
				this,
				&ThisClass::HandleLeaveSessionCompleted);
		}
	}

	if (bWasSuccessful && ReturnMapPath.IsEmpty() == false)
	{
		ClientTravel(ReturnMapPath, TRAVEL_Absolute);
	}
}
