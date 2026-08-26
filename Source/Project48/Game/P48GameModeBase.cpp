// P48GameModeBase.cpp


#include "P48GameModeBase.h"

#include "P48GameStateBase.h"


void AP48GameModeBase::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);
	
	UE_LOG(LogTemp, Warning, TEXT("Player Login: %s"), *GetNameSafe(NewPlayer));
	UE_LOG(LogTemp, Log, TEXT("Player Count: %d"), GetNumPlayers());
	
	CheckStartCondition();
}

void AP48GameModeBase::Logout(AController* Exit)
{
	const FString ExistingPlayerName = GetNameSafe(Exit);
	const int32 RemainingPlayerCount = FMath::Max(0, GetNumPlayers() - 1);
	Super::Logout(Exit);
	
	UE_LOG(LogTemp, Warning, TEXT("Player Logout: %s"), *ExistingPlayerName);
	UE_LOG(LogTemp, Warning, TEXT("Remaining Player Count: %d"), RemainingPlayerCount);
	
	// 카운트다운 중 플레이어 수가 최소 시작 플레이어 수 보다 적어진다면 타이머 리셋 및 MatchPhase Waiting으로 변경
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == true
		&& P48GameState->MatchPhase == EP48MatchPhase::Countdown
		&& RemainingPlayerCount < MinPlayersToStart)
	{
		GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
		P48GameState->SetMatchPhase(EP48MatchPhase::Waiting);
		UE_LOG(LogTemp, Warning, TEXT("[Server] Countdown canceled: not enough players!"));
	}
}

void AP48GameModeBase::CheckStartCondition()
{
	if (GetNumPlayers() < MinPlayersToStart)
	{
		return;
	}
	
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false)
	{
		return;
	}
	
	if (P48GameState->MatchPhase != EP48MatchPhase::Waiting)
	{
		return;
	}
	
	UE_LOG(LogTemp,Warning,TEXT("[Server] Start condition met: %d/%d"),GetNumPlayers(),MinPlayersToStart);
	StartCountdown();
}

void AP48GameModeBase::StartCountdown()
{
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false)
	{
		return;
	}
	
	P48GameState->SetMatchPhase(EP48MatchPhase::Countdown);
	
	UE_LOG(LogTemp,Warning,TEXT("[Server] Countdown started: %.1f sec"),CountdownDuration);
	
	GetWorldTimerManager().SetTimer(
		CountdownTimerHandle,
		this,
		&ThisClass::StartMatch,
		CountdownDuration,
		false);
}

void AP48GameModeBase::StartMatch()
{
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false)
	{
		return;
	}
	
	P48GameState->SetMatchPhase(EP48MatchPhase::Playing);
	
	UE_LOG(LogTemp,Warning,TEXT("[Server] Match started"));
}
