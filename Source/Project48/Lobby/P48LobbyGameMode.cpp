#include "P48LobbyGameMode.h"

#include "P48LobbyGameState.h"
#include "P48LobbyPlayerController.h"
#include "Project48/Character/P48PlayerState.h"
#include "Project48/Online/P48MatchmakingSubsystem.h"

AP48LobbyGameMode::AP48LobbyGameMode()
{
	GameStateClass = AP48LobbyGameState::StaticClass();
	PlayerControllerClass = AP48LobbyPlayerController::StaticClass();
	PlayerStateClass = AP48PlayerState::StaticClass();
	DefaultPawnClass = nullptr;
	bUseSeamlessTravel = false;
}

void AP48LobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer || bCreateSessionOnDedicatedServer == false)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UP48MatchmakingSubsystem* MatchmakingSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UP48MatchmakingSubsystem>() : nullptr;

	if (IsValid(MatchmakingSubsystem)
		&& MatchmakingSubsystem->HasActiveSession() == false)
	{
		MatchmakingSubsystem->CreateSession(MaxLobbyPlayers, true);
	}
}

void AP48LobbyGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);

	AP48PlayerState* JoinedPlayerState =
		NewPlayer ? NewPlayer->GetPlayerState<AP48PlayerState>() : nullptr;
	if (IsValid(JoinedPlayerState) == false)
	{
		return;
	}

	JoinedPlayerState->SetReady(false);
	if (HostPlayerState.IsValid() == false)
	{
		HostPlayerState = JoinedPlayerState;
	}

	RefreshLobbyState();
}

void AP48LobbyGameMode::Logout(AController* Exiting)
{
	AP48PlayerState* ExitingPlayerState =
		Exiting ? Exiting->GetPlayerState<AP48PlayerState>() : nullptr;
	if (HostPlayerState.Get() == ExitingPlayerState)
	{
		HostPlayerState.Reset();
	}

	Super::Logout(Exiting);
	// Logout runs before PlayerState destruction removes it from PlayerArray.
	// Exclude the departing player before rebuilding the roster or choosing a host.
	if (AP48LobbyGameState* LobbyGameState = GetGameState<AP48LobbyGameState>())
	{
		if (IsValid(ExitingPlayerState))
		{
			LobbyGameState->RemovePlayerState(ExitingPlayerState);
		}
	}
	RefreshLobbyState();
}

bool AP48LobbyGameMode::CanPlayerChangeReady(
	const AP48LobbyPlayerController* PlayerController) const
{
	return bTravelStarted == false
		&& IsValid(PlayerController)
		&& IsHostController(PlayerController) == false;
}

void AP48LobbyGameMode::NotifyLobbyReadyStateChanged()
{
	RefreshLobbyState();
}

void AP48LobbyGameMode::RequestStartGame(
	AP48LobbyPlayerController* RequestingController)
{
	if (bTravelStarted || IsHostController(RequestingController) == false)
	{
		return;
	}

	RefreshLobbyState();

	const AP48LobbyGameState* LobbyGameState =
		GetGameState<AP48LobbyGameState>();
	if (IsValid(LobbyGameState) == false
		|| LobbyGameState->CanHostStartGame() == false)
	{
		return;
	}

	TravelToGameMap();
}

void AP48LobbyGameMode::RefreshLobbyState()
{
	AP48LobbyGameState* LobbyGameState =
		GetGameState<AP48LobbyGameState>();
	if (IsValid(LobbyGameState) == false)
	{
		return;
	}

	if (HostPlayerState.IsValid() == false)
	{
		for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
		{
			AP48PlayerState* Candidate = Cast<AP48PlayerState>(PlayerState);
			if (IsValid(Candidate))
			{
				HostPlayerState = Candidate;
				Candidate->SetReady(false);
				break;
			}
		}
	}

	LobbyGameState->RebuildLobbyState(HostPlayerState.Get());
}

bool AP48LobbyGameMode::IsHostController(
	const AP48LobbyPlayerController* PlayerController) const
{
	return IsValid(PlayerController)
		&& PlayerController->GetPlayerState<AP48PlayerState>() == HostPlayerState.Get();
}

void AP48LobbyGameMode::TravelToGameMap()
{
	if (GameMap.IsNull())
	{
		return;
	}

	const FString MapPackageName =
		GameMap.ToSoftObjectPath().GetLongPackageName();
	if (MapPackageName.IsEmpty())
	{
		return;
	}

	bTravelStarted = true;
	const int32 ExpectedPlayers = GetNumPlayers();
	const FString TravelURL = FString::Printf(
		TEXT("%s?StartedFromLobby=1?ExpectedPlayers=%d"),
		*MapPackageName,
		ExpectedPlayers);

	GetWorld()->ServerTravel(TravelURL, false);
}
