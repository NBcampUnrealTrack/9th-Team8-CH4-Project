#include "P48LobbyTravelGameMode.h"

#include "Kismet/GameplayStatics.h"
#include "Project48/Character/P48PlayerState.h"

void AP48LobbyTravelGameMode::InitGame(
	const FString& MapName,
	const FString& Options,
	FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	bStartedFromLobby =
		UGameplayStatics::ParseOption(Options, TEXT("StartedFromLobby")) == TEXT("1");
	ExpectedPlayers = FCString::Atoi(
		*UGameplayStatics::ParseOption(Options, TEXT("ExpectedPlayers")));
}

void AP48LobbyTravelGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);

	if (bStartedFromLobby == false)
	{
		return;
	}

	AP48PlayerState* JoinedPlayerState =
		NewPlayer ? NewPlayer->GetPlayerState<AP48PlayerState>() : nullptr;
	if (IsValid(JoinedPlayerState) == false)
	{
		return;
	}

	JoinedPlayerState->SetReady(true);

	if (ExpectedPlayers <= 0 || GetNumPlayers() >= ExpectedPlayers)
	{
		NotifyPlayerReadyStateChanged();
	}
}
