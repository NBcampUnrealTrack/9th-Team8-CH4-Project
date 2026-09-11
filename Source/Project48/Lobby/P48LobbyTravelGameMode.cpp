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

FString AP48LobbyTravelGameMode::InitNewPlayer(
	APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	const FString SuperError = Super::InitNewPlayer(
		NewPlayerController, UniqueId, Options, Portal);
	if (!SuperError.IsEmpty()) return SuperError;

	const FString Nickname = UGameplayStatics::ParseOption(
		Options, TEXT("Nickname")).TrimStartAndEnd();
	if (!Nickname.IsEmpty())
	{
		ChangeName(NewPlayerController, Nickname, false);
	}
	return FString();
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
