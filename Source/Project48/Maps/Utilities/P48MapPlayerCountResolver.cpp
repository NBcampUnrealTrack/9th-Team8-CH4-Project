#include "P48MapPlayerCountResolver.h"

#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

int32 FP48MapPlayerCountResolver::Resolve(const UWorld* World)
{
	if (!World)
	{
		return 0;
	}

	if (AGameModeBase* GameMode = World->GetAuthGameMode())
	{
		return FMath::Max(0, GameMode->GetNumPlayers());
	}

	const AGameStateBase* GameState = World->GetGameState();
	if (!GameState)
	{
		return 0;
	}

	int32 PlayerCount = 0;
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (IsValid(PlayerState) && !PlayerState->IsOnlyASpectator())
		{
			++PlayerCount;
		}
	}
	return PlayerCount;
}
