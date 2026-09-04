// P48SurvivalGameMode.cpp


#include "P48SurvivalGameMode.h"

#include "P48GameStateBase.h"
#include "Project48/Character/P48PlayerState.h"


int32 AP48SurvivalGameMode::GetAliveParticipantCount() const
{
	const AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false)
	{
		return 0;
	}
	
	int32 AliveCount = 0;
	
	for (APlayerState* PlayerState : P48GameState->PlayerArray)
	{
		const AP48PlayerState* P48PlayerState = Cast<AP48PlayerState>(PlayerState);
		if (IsValid(P48PlayerState)
			&& P48PlayerState->IsMatchParticipant()
			&& P48PlayerState->IsAlive())
		{
			AliveCount++;
		}
	}
	
	return AliveCount;
}

AP48PlayerState* AP48SurvivalGameMode::LastAliveParticipant() const
{
	const AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false)
	{
		return nullptr;
	}

	AP48PlayerState* LastAlivePlayer = nullptr;
	int32 AliveCount = 0;

	for (APlayerState* PlayerState : P48GameState->PlayerArray)
	{
		AP48PlayerState* P48PlayerState = Cast<AP48PlayerState>(PlayerState);
		if (IsValid(P48PlayerState)
			&& P48PlayerState->IsMatchParticipant()
			&& P48PlayerState->IsAlive())
		{
			LastAlivePlayer = P48PlayerState;
			AliveCount++;
		}
	}

	return AliveCount == 1 ? LastAlivePlayer : nullptr;
}

void AP48SurvivalGameMode::NotifyPlayerEliminated(AP48PlayerState* EliminatedPlayer)
{
	if (HasAuthority() == false)
	{
		return;
	}

	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false
		|| P48GameState->MatchPhase != EP48MatchPhase::Playing)
	{
		return;
	}

	if (IsValid(EliminatedPlayer) == false
		|| EliminatedPlayer->IsMatchParticipant() == false
		|| EliminatedPlayer->IsAlive() == false)
	{
		return;
	}

	EliminatedPlayer->SetAlive(false);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Server] Player eliminated: %s, Alive participants: %d"),
		*EliminatedPlayer->GetPlayerName(),
		GetAliveParticipantCount());

	RoundEndCheck();
}

void AP48SurvivalGameMode::RoundEndCheck()
{
	if (bRoundEndCheck)
	{
		return;
	}

	bRoundEndCheck = true;

	GetWorldTimerManager().SetTimer(
		RoundEndCheckTimerHandle,
		this,
		&ThisClass::CheckRoundEndCondition,
		RoundEndCheckDelay,
		false);
}

void AP48SurvivalGameMode::CheckRoundEndCondition()
{
	if (HasAuthority() == false)
	{
		bRoundEndCheck = false;
		return;
	}

	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();

	if (IsValid(P48GameState) == false || P48GameState->MatchPhase != EP48MatchPhase::Playing)
	{
		bRoundEndCheck = false;
		return;
	}

	const int32 AliveCount = GetAliveParticipantCount();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Server] Round end check: Alive participants = %d"),
		AliveCount);

	if (AliveCount > 1)
	{
		bRoundEndCheck = false;
		return;
	}

	FinishSurvivalRound(AliveCount == 1 ? LastAliveParticipant() : nullptr);
}

void AP48SurvivalGameMode::FinishSurvivalRound(AP48PlayerState* Winner)
{
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (HasAuthority() == false
		|| IsValid(P48GameState) == false
		|| P48GameState->MatchPhase != EP48MatchPhase::Playing)
	{
		return;
	}

	if (IsValid(Winner))
	{
		P48GameState->SetRoundWinner(Winner); //라운드 승자
		Winner->AddRoundWin();
	}
	else
	{
		P48GameState->SetRoundDraw(); //무승부
	}

	StartRoundEnd();
}

void AP48SurvivalGameMode::ResetParticipantsForRound()
{
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false)
	{
		return;
	}

	for (APlayerState* PlayerState : P48GameState->PlayerArray)
	{
		AP48PlayerState* P48PlayerState = Cast<AP48PlayerState>(PlayerState);
		if (IsValid(P48PlayerState))
		{
			P48PlayerState->ResetForNewRound();
		}
	}
}

void AP48SurvivalGameMode::StartMatch()
{
	if (HasAuthority() == false)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RoundEndCheckTimerHandle);
	bRoundEndCheck = false;

	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false)
	{
		return;
	}
	
	P48GameState->ResetRoundResult();
	ResetParticipantsForRound();
	Super::StartMatch();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Server] Survival round started: Alive participants = %d"),
		GetAliveParticipantCount());
}

void AP48SurvivalGameMode::Logout(AController* Exit)
{
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();

	AP48PlayerState* ExitingPlayerState = IsValid(Exit) ? Exit->GetPlayerState<AP48PlayerState>() : nullptr;

	if (IsValid(P48GameState)
		&& P48GameState->MatchPhase == EP48MatchPhase::Playing
		&& IsValid(ExitingPlayerState)
		&& ExitingPlayerState->IsMatchParticipant()
		&& ExitingPlayerState->IsAlive())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Server] Participant logout treated as elimination: %s"),
			*ExitingPlayerState->GetPlayerName());

		NotifyPlayerEliminated(ExitingPlayerState);
	}

	Super::Logout(Exit);
}