#pragma once

#include "CoreMinimal.h"
#include "Project48/Character/P48PlayerController.h"
#include "P48LobbyPlayerController.generated.h"

UCLASS()
class PROJECT48_API AP48LobbyPlayerController : public AP48PlayerController
{
	GENERATED_BODY()

public:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

private:
	UFUNCTION(Server, Reliable)
	void Server_SetLobbyReady(bool bNewReady);

	UFUNCTION(Server, Reliable)
	void Server_RequestStartGame();

	UFUNCTION()
	void HandleLeaveSessionCompleted(bool bWasSuccessful);

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Travel")
	FString ReturnMapPath = TEXT("/Game/WJS/OnlineTest/L_OnlineTest");
};
