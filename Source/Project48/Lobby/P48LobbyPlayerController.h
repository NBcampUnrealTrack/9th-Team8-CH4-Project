#pragma once

#include "CoreMinimal.h"
#include "Project48/Character/P48PlayerController.h"
#include "P48LobbyPlayerController.generated.h"

class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FP48LobbyMembershipChanged, bool, bIsInLobby);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FP48LobbyRequestFailed, const FText&, Reason);

UCLASS()
class PROJECT48_API AP48LobbyPlayerController : public AP48PlayerController
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Call once on the local controller instead of creating widgets in the level.
	UFUNCTION(BlueprintCallable, Category = "Lobby|UI")
	void InitializeLobbyScreens(TSubclassOf<UUserWidget> MenuScreen, TSubclassOf<UUserWidget> LobbyScreen);

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestCreateLobby(bool bUsePassword, const FString& Password);

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestCreateLobbyWithSettings(const FString& RoomTitle,
		bool bUsePassword, const FString& Password);

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestJoinLobby();

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestJoinLobbyById(int32 RoomId, const FString& Password);

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsInLobby() const { return LobbyRoomId != INDEX_NONE; }

	UFUNCTION(BlueprintPure, Category = "Lobby")
	int32 GetLobbyRoomId() const { return LobbyRoomId; }

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FP48LobbyMembershipChanged OnLobbyMembershipChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FP48LobbyRequestFailed OnLobbyRequestFailed;

	// Server-only mutation; clients receive the state through replication.
	void SetLobbyRoomId(int32 NewRoomId);

	UFUNCTION(Client, Reliable)
	void Client_ReportLobbyRequestFailure(const FText& Reason);

	UFUNCTION(Client, Reliable)
	void Client_TravelToGameServer(const FString& ServerAddress,
		const FString& ReturnAddress, int32 ReturnRoomId, const FGuid& ReturnMemberId);

	UFUNCTION(Client, Reliable)
	void Client_ConfirmLobbyReturn();

	void SetPendingLobbyReturn(int32 RoomId, const FGuid& MemberId);
	bool ConsumePendingLobbyReturn(int32& OutRoomId, FGuid& OutMemberId);

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestLobbyReady(bool bNewReady);

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestStartGame();

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void RequestLeaveLobby();

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsLobbyHost() const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool CanStartLobbyGame() const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsLocalPlayerLobbyReady() const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool CanChangeLobbyReady() const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsCurrentLobbyReturningToLobby() const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	FText GetCurrentLobbyStatusText() const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	FText GetCurrentLobbyPlayerListText() const;

private:
	void RefreshLobbyScreen();

	UPROPERTY(Transient)
	TSubclassOf<UUserWidget> MenuScreenClass;

	UPROPERTY(Transient)
	TSubclassOf<UUserWidget> LobbyScreenClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ActiveLobbyScreen;

	UFUNCTION(Server, Reliable)
	void Server_CreateLobby(bool bUsePassword, const FString& Password);

	UFUNCTION(Server, Reliable)
	void Server_CreateLobbyWithSettings(const FString& RoomTitle,
		bool bUsePassword, const FString& Password);

	UFUNCTION(Server, Reliable)
	void Server_JoinLobby();

	UFUNCTION(Server, Reliable)
	void Server_JoinLobbyById(int32 RoomId, const FString& Password);

	UFUNCTION(Server, Reliable)
	void Server_LeaveLobby();

	UFUNCTION(Server, Reliable)
	void Server_SetLobbyReady(bool bNewReady);

	UFUNCTION(Server, Reliable)
	void Server_RequestStartGame();

	UFUNCTION()
	void OnRep_LobbyMembership();

	UPROPERTY(ReplicatedUsing = OnRep_LobbyMembership)
	int32 LobbyRoomId = INDEX_NONE;

	int32 PendingReturnRoomId = INDEX_NONE;
	FGuid PendingReturnMemberId;
};
