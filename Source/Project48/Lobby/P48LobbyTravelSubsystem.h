#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Net/Core/Connection/NetEnums.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "P48LobbyTravelSubsystem.generated.h"

class UNetDriver;

UCLASS()
class PROJECT48_API UP48LobbyTravelSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Called by the lobby controller immediately before traveling to the game server.
	bool StoreLobbyReturnContext(const FString& Address, int32 RoomId,
		const FGuid& MemberId);
	bool BeginGameServerTravel(const FString& Destination);
	void ConfirmGameServerArrival();

	UFUNCTION(BlueprintPure, Category = "Lobby|Travel")
	bool HasLobbyReturnContext() const;

	// Game-end code only needs to call this once for each local client.
	UFUNCTION(BlueprintCallable, Category = "Lobby|Travel")
	bool ReturnToLobby();

	void ClearLobbyReturnContext();

private:
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
		ENetworkFailure::Type FailureType, const FString& ErrorString);
	bool TickGameServerRetry(float DeltaTime);
	bool TryGameServerTravel();
	void ClearGameServerRetry();

	FString LobbyReturnAddress;
	int32 LobbyReturnRoomId = INDEX_NONE;
	FGuid LobbyReturnMemberId;
	bool bReturnInProgress = false;

	FString PendingGameServerDestination;
	bool bGameServerTravelPending = false;
	bool bGameServerConnectionAttemptInProgress = false;
	int32 GameServerRetryAttempt = 0;
	double NextGameServerRetryTime = 0.0;
	double GameServerRetryDeadline = 0.0;
	FDelegateHandle NetworkFailureHandle;
	FTSTicker::FDelegateHandle RetryTickerHandle;
};
