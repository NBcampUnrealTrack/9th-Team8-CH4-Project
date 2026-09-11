#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "P48LobbyGameMode.generated.h"

class AP48LobbyPlayerController;
class AP48PlayerState;
class UWorld;

USTRUCT()
struct FP48LobbyTravelMember
{
	GENERATED_BODY()

	// Server-generated record ID, not an authentication credential.
	FGuid MemberId;
	FString PlayerName;
	bool bIsHost = false;
	bool bAtGameServer = false;

	// Used only while disconnecting from the lobby. Never retain the old actor strongly.
	TWeakObjectPtr<AP48PlayerState> LobbyPlayerState;
};

USTRUCT()
struct FP48LobbyRoomState
{
	GENERATED_BODY()

	UPROPERTY()
	int32 RoomId = INDEX_NONE;

	UPROPERTY()
	TArray<TObjectPtr<AP48PlayerState>> Participants;

	UPROPERTY()
	TObjectPtr<AP48PlayerState> HostPlayerState = nullptr;

	UPROPERTY()
	bool bMatchStarting = false;

	// Server-only secret. Never copy this value into the replicated GameState.
	FString Password;
	FString RoomTitle;

	// Kept on the lobby server after its PlayerState actors are destroyed.
	UPROPERTY()
	TArray<FP48LobbyTravelMember> TravelMembers;

	FString AssignedGameServerAddress;
	bool bReturningToLobby = false;

	void RecordGameHandoff(const FString& Destination);
	bool RecordMemberHandoff(AP48PlayerState* State);
	const FP48LobbyTravelMember* FindTravelMember(const AP48PlayerState* State) const;
	bool RestoreTravelMember(const FGuid& MemberId, AP48PlayerState* State);
	bool AreAllTravelMembersBack() const;
	void RemoveParticipant(AP48PlayerState* State, bool bPreserveTravelRecord);
	int32 GetMemberCount() const;
	bool IsEmpty() const { return Participants.IsEmpty() && TravelMembers.IsEmpty(); }
};

UCLASS()
class PROJECT48_API AP48LobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AP48LobbyGameMode();

	virtual void BeginPlay() override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController,
		const FUniqueNetIdRepl& UniqueId, const FString& Options,
		const FString& Portal = TEXT("")) override;
	virtual void OnPostLogin(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	bool CanPlayerChangeReady(
		const AP48LobbyPlayerController* PlayerController) const;
	void RequestLobbyReady(AP48LobbyPlayerController* PlayerController, bool bNewReady);
	void NotifyLobbyReadyStateChanged();
	void RequestStartGame(AP48LobbyPlayerController* RequestingController);
	bool RequestCreateLobby(AP48LobbyPlayerController* PlayerController,
		bool bUsePassword, const FString& Password, FText& OutError);
	bool RequestCreateLobbyWithSettings(AP48LobbyPlayerController* PlayerController,
		const FString& RoomTitle, bool bUsePassword, const FString& Password,
		FText& OutError);
	bool RequestJoinLobby(AP48LobbyPlayerController* PlayerController, FText& OutError);
	bool RequestJoinLobbyById(AP48LobbyPlayerController* PlayerController,
		int32 RoomId, const FString& Password, FText& OutError);
	void LeaveLobby(AP48LobbyPlayerController* PlayerController);
	bool IsLobbyParticipant(const AP48LobbyPlayerController* PlayerController) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Session", meta = (ClampMin = "2"))
	int32 MaxLobbyPlayers = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Session", meta = (ClampMin = "1"))
	int32 MaxLobbyRooms = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Travel")
	TSoftObjectPtr<UWorld> GameMap;

	// Legacy single-server fallback. Prefer GameServerAddresses for concurrent rooms.
	// May also be supplied on the command line: -LobbyGameServerAddress=host:port
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Travel")
	FString GameServerAddress;

	// Addresses of already running game-server processes. One active room reserves one address.
	// Command-line override: -LobbyGameServerAddresses=host:17778,host:17779,host:17780
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Travel")
	TArray<FString> GameServerAddresses = {
		TEXT("127.0.0.1:17778"),
		TEXT("127.0.0.1:17779"),
		TEXT("127.0.0.1:17780")
	};

	// Address clients use when returning from the game server.
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Travel")
	FString LobbyReturnAddress = TEXT("127.0.0.1:17777");

	// Starts when the first game client returns. Missing clients are released after this delay.
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Travel", meta = (ClampMin = "5.0"))
	float LobbyReturnGracePeriodSeconds = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Session")
	bool bCreateSessionOnDedicatedServer = true;

private:
	// TEMP: room-retention verification. Remove after the multi-client test.
	void LogRoomRetentionSnapshot(const TCHAR* Event) const;
	void RefreshLobbyState();
	FP48LobbyRoomState* FindRoom(int32 RoomId);
	const FP48LobbyRoomState* FindRoomForPlayer(const AP48PlayerState* PlayerState) const;
	bool IsHostController(
		const AP48LobbyPlayerController* PlayerController) const;
	FString FindAvailableGameServerAddress() const;
	bool RestoreReturningPlayer(AP48LobbyPlayerController* PlayerController,
		AP48PlayerState* PlayerState);
	void StartLobbyReturnGracePeriod(int32 RoomId);
	void HandleLobbyReturnTimeout(int32 RoomId);
	void FinalizeLobbyReturn(FP48LobbyRoomState& Room, bool bDiscardMissingMembers);
	void RemoveLobbyParticipant(AP48PlayerState* PlayerState, bool bPreserveTravelRecord = false);

	UPROPERTY(Transient)
	TArray<FP48LobbyRoomState> LobbyRooms;

	TMap<int32, FTimerHandle> LobbyReturnTimers;

};
