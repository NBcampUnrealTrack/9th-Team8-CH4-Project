#include "P48LobbyGameState.h"

#include "Net/UnrealNetwork.h"
#include "Project48/Character/P48PlayerState.h"

void AP48LobbyGameState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AP48LobbyGameState, LobbyPlayers);
	DOREPLIFETIME(AP48LobbyGameState, LobbyRooms);
}

TArray<FP48LobbyPlayerEntry> AP48LobbyGameState::GetLobbyPlayers() const
{
	return LobbyPlayers;
}

TArray<FP48LobbyPlayerEntry> AP48LobbyGameState::GetLobbyPlayersForRoom(int32 RoomId) const
{
	return LobbyPlayers.FilterByPredicate([RoomId](const FP48LobbyPlayerEntry& Entry)
	{
		return Entry.RoomId == RoomId;
	});
}

bool AP48LobbyGameState::GetLobbyPlayerForRoomSlot(
	int32 RoomId, int32 SlotNumber, FP48LobbyPlayerEntry& OutPlayer) const
{
	OutPlayer = FP48LobbyPlayerEntry{};
	if (RoomId < 1 || SlotNumber < 1 || SlotNumber > 8)
	{
		return false;
	}

	TArray<const FP48LobbyPlayerEntry*> RoomPlayers;
	RoomPlayers.Reserve(LobbyPlayers.Num());
	for (const FP48LobbyPlayerEntry& Entry : LobbyPlayers)
	{
		if (Entry.RoomId == RoomId)
		{
			RoomPlayers.Add(&Entry);
		}
	}

	RoomPlayers.StableSort([](const FP48LobbyPlayerEntry& Left,
		const FP48LobbyPlayerEntry& Right)
	{
		return Left.bIsHost && !Right.bIsHost;
	});

	const int32 PlayerIndex = SlotNumber - 1;
	if (!RoomPlayers.IsValidIndex(PlayerIndex))
	{
		return false;
	}

	OutPlayer = *RoomPlayers[PlayerIndex];
	return true;
}

TArray<FP48LobbyRoomInfo> AP48LobbyGameState::GetLobbyRooms() const
{
	return LobbyRooms;
}

bool AP48LobbyGameState::IsLobbyRoomSlotOccupied(int32 RoomId) const
{
	return RoomId >= 1 && RoomId <= 3
		&& LobbyRooms.ContainsByPredicate([RoomId](const FP48LobbyRoomInfo& Room)
		{
			return Room.RoomId == RoomId;
		});
}

FText AP48LobbyGameState::GetLobbyPlayerListText() const
{
	return GetLobbyPlayerListTextForRoom(
		LobbyPlayers.IsEmpty() ? INDEX_NONE : LobbyPlayers[0].RoomId);
}

FText AP48LobbyGameState::GetLobbyPlayerListTextForRoom(int32 RoomId) const
{
	TArray<FString> PlayerLines;
	PlayerLines.Reserve(LobbyPlayers.Num());

	for (const FP48LobbyPlayerEntry& Entry : LobbyPlayers)
	{
		if (Entry.RoomId != RoomId)
		{
			continue;
		}
		const TCHAR* StateText = Entry.bIsHost
			? TEXT("Host")
			: (Entry.bIsReady ? TEXT("Ready") : TEXT("Not Ready"));
		if (Entry.bTravelRequested)
		{
			StateText = Entry.bIsHost ? TEXT("Host / Travel Requested") : TEXT("Travel Requested");
		}

		PlayerLines.Add(FString::Printf(
			TEXT("%s  [%s]"),
			*Entry.PlayerName,
			StateText));
	}

	return FText::FromString(FString::Join(PlayerLines, TEXT("\n")));
}

AP48PlayerState* AP48LobbyGameState::GetHostPlayerState() const
{
	for (const FP48LobbyPlayerEntry& Entry : LobbyPlayers)
	{
		if (Entry.bIsHost)
		{
			return Entry.PlayerState;
		}
	}
	return nullptr;
}

bool AP48LobbyGameState::IsHost(const APlayerState* PlayerState) const
{
	return IsValid(PlayerState) && LobbyPlayers.ContainsByPredicate(
		[PlayerState](const FP48LobbyPlayerEntry& Entry)
		{
			return Entry.PlayerState == PlayerState && Entry.bIsHost;
		});
}

bool AP48LobbyGameState::CanHostStartGame() const
{
	for (const FP48LobbyPlayerEntry& Entry : LobbyPlayers)
	{
		if (Entry.bIsHost)
		{
			return CanPlayerStartGame(Entry.PlayerState);
		}
	}
	return false;
}

bool AP48LobbyGameState::CanPlayerStartGame(const APlayerState* PlayerState) const
{
	// Preserved records intentionally have no lobby PlayerState after disconnect.
	if (!IsValid(PlayerState)) return false;
	const FP48LobbyPlayerEntry* HostEntry = LobbyPlayers.FindByPredicate(
		[PlayerState](const FP48LobbyPlayerEntry& Entry)
		{
			return Entry.PlayerState == PlayerState && Entry.bIsHost && !Entry.bTravelRequested;
		});
	if (!HostEntry)
	{
		return false;
	}
	const FP48LobbyRoomInfo* HostRoom = LobbyRooms.FindByPredicate(
		[HostEntry](const FP48LobbyRoomInfo& Candidate)
		{
			return Candidate.RoomId == HostEntry->RoomId;
		});
	if (!HostRoom || HostRoom->bIsReturningToLobby || HostRoom->bIsGameServerResetting)
	{
		return false;
	}

	int32 GuestCount = 0;
	for (const FP48LobbyPlayerEntry& Entry : LobbyPlayers)
	{
		if (Entry.RoomId == HostEntry->RoomId && !Entry.bIsHost)
		{
			++GuestCount;
			if (!Entry.bIsReady)
			{
				return false;
			}
		}
	}
	return GuestCount > 0;
}

bool AP48LobbyGameState::IsLobbyOpen() const
{
	return !LobbyRooms.IsEmpty();
}

bool AP48LobbyGameState::IsRoomReturningToLobby(int32 RoomId) const
{
	const FP48LobbyRoomInfo* Room = LobbyRooms.FindByPredicate(
		[RoomId](const FP48LobbyRoomInfo& Candidate)
		{
			return Candidate.RoomId == RoomId;
		});
	return Room && Room->bIsReturningToLobby;
}

FText AP48LobbyGameState::GetRoomStatusText(int32 RoomId) const
{
	const FP48LobbyRoomInfo* Room = LobbyRooms.FindByPredicate(
		[RoomId](const FP48LobbyRoomInfo& Candidate)
		{
			return Candidate.RoomId == RoomId;
		});
	if (!Room) return FText::GetEmpty();
	if (Room->bIsReturningToLobby)
	{
		return FText::FromString(TEXT("Waiting for the game players to return"));
	}
	if (Room->bIsGameServerResetting)
	{
		return FText::FromString(TEXT("Waiting for the game server to reset"));
	}
	return Room->bHasGameServer
		? FText::FromString(TEXT("Game in progress"))
		: FText::FromString(TEXT("Waiting for players"));
}

void AP48LobbyGameState::RebuildLobbyState(
	const TArray<FP48LobbyPlayerEntry>& NewLobbyPlayers,
	const TArray<FP48LobbyRoomInfo>& NewLobbyRooms)
{
	if (!HasAuthority()) return;
	LobbyPlayers = NewLobbyPlayers;
	LobbyRooms = NewLobbyRooms;
	ForceNetUpdate();
	OnLobbyStateChanged.Broadcast();
}

void AP48LobbyGameState::OnRep_LobbyState()
{
	OnLobbyStateChanged.Broadcast();
}
