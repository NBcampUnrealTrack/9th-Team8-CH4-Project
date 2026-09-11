#pragma once

#include "CoreMinimal.h"
#include "Project48/Game/P48SurvivalGameMode.h"
#include "P48LobbyTravelGameMode.generated.h"

UCLASS()
class PROJECT48_API AP48LobbyTravelGameMode : public AP48SurvivalGameMode
{
	GENERATED_BODY()

public:
	virtual void InitGame(
		const FString& MapName,
		const FString& Options,
		FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController,
		const FUniqueNetIdRepl& UniqueId, const FString& Options,
		const FString& Portal = TEXT("")) override;
	virtual void OnPostLogin(AController* NewPlayer) override;

private:
	bool bStartedFromLobby = false;
	int32 ExpectedPlayers = 0;
};
