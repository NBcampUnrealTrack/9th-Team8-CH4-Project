#include "P48LobbyGameState.h"

#include "Net/UnrealNetwork.h"
#include "Project48/Character/P48PlayerState.h"

void AP48LobbyGameState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AP48LobbyGameState, HostPlayerState);
	DOREPLIFETIME(AP48LobbyGameState, LobbyPlayers);
	DOREPLIFETIME(AP48LobbyGameState, bCanHostStartGame);
}

TArray<FP48LobbyPlayerEntry> AP48LobbyGameState::GetLobbyPlayers() const
{
	return LobbyPlayers;
}

FText AP48LobbyGameState::GetLobbyPlayerListText() const
{
	TArray<FString> PlayerLines;
	PlayerLines.Reserve(LobbyPlayers.Num());

	for (const FP48LobbyPlayerEntry& Entry : LobbyPlayers)
	{
		const TCHAR* StateText = Entry.bIsHost
			? TEXT("Host")
			: (Entry.bIsReady ? TEXT("Ready") : TEXT("Not Ready"));

		PlayerLines.Add(FString::Printf(
			TEXT("%s  [%s]"),
			*Entry.PlayerName,
			StateText));
	}

	return FText::FromString(FString::Join(PlayerLines, TEXT("\n")));
}

AP48PlayerState* AP48LobbyGameState::GetHostPlayerState() const
{
	return HostPlayerState;
}

bool AP48LobbyGameState::IsHost(const APlayerState* PlayerState) const
{
	return IsValid(PlayerState) && PlayerState == HostPlayerState;
}

bool AP48LobbyGameState::CanHostStartGame() const
{
	return bCanHostStartGame;
}

void AP48LobbyGameState::RebuildLobbyState(AP48PlayerState* NewHostPlayerState)
{
	if (HasAuthority() == false)
	{
		return;
	}

	HostPlayerState = NewHostPlayerState;
	LobbyPlayers.Reset();

	int32 GuestCount = 0;
	bool bAllGuestsReady = true;

	for (APlayerState* PlayerState : PlayerArray)
	{
		AP48PlayerState* P48PlayerState = Cast<AP48PlayerState>(PlayerState);
		if (IsValid(P48PlayerState) == false)
		{
			continue;
		}

		FP48LobbyPlayerEntry& Entry = LobbyPlayers.AddDefaulted_GetRef();
		Entry.PlayerState = P48PlayerState;
		Entry.PlayerName = P48PlayerState->GetPlayerName();
		Entry.bIsHost = P48PlayerState == HostPlayerState;
		Entry.bIsReady = Entry.bIsHost == false && P48PlayerState->IsReady();

		if (Entry.bIsHost == false)
		{
			++GuestCount;
			bAllGuestsReady &= Entry.bIsReady;
		}
	}

	bCanHostStartGame = IsValid(HostPlayerState)
		&& GuestCount > 0
		&& bAllGuestsReady;

	OnLobbyStateChanged.Broadcast();
}

void AP48LobbyGameState::OnRep_LobbyState()
{
	OnLobbyStateChanged.Broadcast();
}
