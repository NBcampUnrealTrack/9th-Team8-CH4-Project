#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "P48LobbyGameState.generated.h"

class AP48PlayerState;

USTRUCT(BlueprintType)
struct FP48LobbyRoomInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 RoomId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	FString RoomTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bRequiresPassword = false;

	// Assignment only; this is not a game-server health or match-start acknowledgement.
	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bHasGameServer = false;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bIsReturningToLobby = false;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bIsGameServerResetting = false;
};

USTRUCT(BlueprintType)
struct FP48LobbyPlayerEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	TObjectPtr<AP48PlayerState> PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 RoomId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bIsHost = false;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bIsReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bTravelRequested = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FP48LobbyStateChanged);

UCLASS()
class PROJECT48_API AP48LobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	TArray<FP48LobbyPlayerEntry> GetLobbyPlayers() const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	TArray<FP48LobbyPlayerEntry> GetLobbyPlayersForRoom(int32 RoomId) const;

	// Display slots are one-based for direct use by the eight fixed lobby UI slots.
	// The host is placed first and the remaining players retain their replicated order.
	UFUNCTION(BlueprintPure, Category = "Lobby|Players")
	bool GetLobbyPlayerForRoomSlot(int32 RoomId, int32 SlotNumber,
		FP48LobbyPlayerEntry& OutPlayer) const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	TArray<FP48LobbyRoomInfo> GetLobbyRooms() const;

	// Fixed lobby slots are occupied only while their room info exists.
	UFUNCTION(BlueprintPure, Category = "Lobby|Rooms")
	bool IsLobbyRoomSlotOccupied(int32 RoomId) const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	FText GetLobbyPlayerListText() const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	FText GetLobbyPlayerListTextForRoom(int32 RoomId) const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	AP48PlayerState* GetHostPlayerState() const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsHost(const APlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool CanHostStartGame() const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool CanPlayerStartGame(const APlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsLobbyOpen() const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsRoomReturningToLobby(int32 RoomId) const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	FText GetRoomStatusText(int32 RoomId) const;

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FP48LobbyStateChanged OnLobbyStateChanged;

	void RebuildLobbyState(const TArray<FP48LobbyPlayerEntry>& NewLobbyPlayers,
		const TArray<FP48LobbyRoomInfo>& NewLobbyRooms);

private:
	UFUNCTION()
	void OnRep_LobbyState();

	UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
	TArray<FP48LobbyPlayerEntry> LobbyPlayers;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
	TArray<FP48LobbyRoomInfo> LobbyRooms;
};
