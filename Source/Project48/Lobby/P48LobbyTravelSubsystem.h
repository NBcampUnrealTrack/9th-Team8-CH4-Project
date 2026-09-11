#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "P48LobbyTravelSubsystem.generated.h"

UCLASS()
class PROJECT48_API UP48LobbyTravelSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Called by the lobby controller immediately before traveling to the game server.
	bool StoreLobbyReturnContext(const FString& Address, int32 RoomId,
		const FGuid& MemberId);

	UFUNCTION(BlueprintPure, Category = "Lobby|Travel")
	bool HasLobbyReturnContext() const;

	// Game-end code only needs to call this once for each local client.
	UFUNCTION(BlueprintCallable, Category = "Lobby|Travel")
	bool ReturnToLobby();

	void ClearLobbyReturnContext();

private:
	FString LobbyReturnAddress;
	int32 LobbyReturnRoomId = INDEX_NONE;
	FGuid LobbyReturnMemberId;
	bool bReturnInProgress = false;
};
