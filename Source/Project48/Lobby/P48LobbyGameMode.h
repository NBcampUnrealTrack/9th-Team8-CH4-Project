#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "P48LobbyGameMode.generated.h"

class AP48LobbyPlayerController;
class AP48PlayerState;
class UWorld;

UCLASS()
class PROJECT48_API AP48LobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AP48LobbyGameMode();

	virtual void BeginPlay() override;
	virtual void OnPostLogin(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	bool CanPlayerChangeReady(
		const AP48LobbyPlayerController* PlayerController) const;
	void NotifyLobbyReadyStateChanged();
	void RequestStartGame(AP48LobbyPlayerController* RequestingController);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Session", meta = (ClampMin = "2"))
	int32 MaxLobbyPlayers = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Travel")
	TSoftObjectPtr<UWorld> GameMap;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Session")
	bool bCreateSessionOnDedicatedServer = true;

private:
	void RefreshLobbyState();
	bool IsHostController(
		const AP48LobbyPlayerController* PlayerController) const;
	void TravelToGameMap();

	TWeakObjectPtr<AP48PlayerState> HostPlayerState;
	bool bTravelStarted = false;
};
