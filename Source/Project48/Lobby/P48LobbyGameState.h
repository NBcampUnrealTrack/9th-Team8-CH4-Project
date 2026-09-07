#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "P48LobbyGameState.generated.h"

class AP48PlayerState;

USTRUCT(BlueprintType)
struct FP48LobbyPlayerEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	TObjectPtr<AP48PlayerState> PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bIsHost = false;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bIsReady = false;
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
	FText GetLobbyPlayerListText() const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	AP48PlayerState* GetHostPlayerState() const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsHost(const APlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool CanHostStartGame() const;

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FP48LobbyStateChanged OnLobbyStateChanged;

	void RebuildLobbyState(AP48PlayerState* NewHostPlayerState);

private:
	UFUNCTION()
	void OnRep_LobbyState();

	UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
	TObjectPtr<AP48PlayerState> HostPlayerState = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
	TArray<FP48LobbyPlayerEntry> LobbyPlayers;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyState)
	bool bCanHostStartGame = false;
};
