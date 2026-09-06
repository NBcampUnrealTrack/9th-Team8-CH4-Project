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
		if (IsCurrentRoundParticipant(P48PlayerState)
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
		if (IsCurrentRoundParticipant(P48PlayerState)
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

	if (!IsCurrentRoundParticipant(EliminatedPlayer)
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

	// 연속 퇴장을 모으는 동안 기존 사망 판정이 먼저 승자를 확정하지 않도록 한다.
	if (P48GameState->IsTiebreaker()
		&& GetWorldTimerManager().IsTimerActive(TiebreakerLogoutTimerHandle))
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

	// 결정전은 일반 승수 집계 없이 생존자를 최종 승자로 처리한다.
	if (P48GameState->IsTiebreaker())
	{
		if (IsValid(Winner))
		{
			P48GameState->SetRoundWinner(Winner);
			UE_LOG(LogTemp, Log, TEXT("[Server] Tiebreaker winner: %s"), *Winner->GetPlayerName());
			StartMatchEnd(Winner);
		}
		else
		{
			P48GameState->SetRoundDraw();
			UE_LOG(LogTemp, Log, TEXT("[Server] Tiebreaker draw: Scheduling replay"));
			StartRoundEnd();
		}
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

	if (!HasFinishedDefaultRounds())
	{
		StartRoundEnd();
		return;
	}

	if (TryFinishMatchWithSingleLeader())
	{
		return;
	}

	const TArray<AP48PlayerState*> TopPlayers = FindTopRoundWinners();
	if (TopPlayers.Num() > 1)
	{
		StartTiebreaker(TopPlayers);
		return;
	}

	StartMatchDraw();
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
			if (!IsCurrentRoundParticipant(P48PlayerState))
			{
				P48PlayerState->SetAlive(false);
			}
		}
	}
}

void AP48SurvivalGameMode::StartMatch()
{
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();

	if (!HasAuthority() || !IsValid(P48GameState) || P48GameState->MatchPhase != EP48MatchPhase::Countdown)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RoundEndCheckTimerHandle);
	bRoundEndCheck = false;

	P48GameState->ResetRoundResult();
	ResetParticipantsForRound();
	Super::StartMatch();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Server] Survival round started: Alive participants = %d"),
		GetAliveParticipantCount());
}

void AP48SurvivalGameMode::StartMatchEnd(AP48PlayerState* Winner)
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS) || !IsValid(Winner))
	{
		return;
	}

	const bool bPlaying = GS->MatchPhase == EP48MatchPhase::Playing;
	const bool bTiebreakerPreparation = GS->IsTiebreaker()
		&& (GS->MatchPhase == EP48MatchPhase::RoundEnd
			|| GS->MatchPhase == EP48MatchPhase::Countdown);
	if (!bPlaying && !bTiebreakerPreparation)
	{
		return;
	}

	// 매치 종료시 타이머 정리
	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundEndTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundEndCheckTimerHandle);
	GetWorldTimerManager().ClearTimer(TiebreakerLogoutTimerHandle);
	bRoundEndCheck = false;

	GS->SetMatchWinner(Winner);
	GS->SetTiebreaker(false);
	TiebreakerParticipants.Reset();
	GS->SetMatchPhase(EP48MatchPhase::MatchEnd);
}

void AP48SurvivalGameMode::StartMatchDraw()
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS)
		|| GS->MatchPhase == EP48MatchPhase::Waiting
		|| GS->MatchPhase == EP48MatchPhase::MatchEnd)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundEndTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundEndCheckTimerHandle);
	GetWorldTimerManager().ClearTimer(TiebreakerLogoutTimerHandle);
	bRoundEndCheck = false;

	GS->ResetMatchResult();
	GS->SetTiebreaker(false);
	TiebreakerParticipants.Reset();
	GS->SetMatchPhase(EP48MatchPhase::MatchEnd);
	UE_LOG(LogTemp, Log, TEXT("[Server] Match ended in a draw"));
}

void AP48SurvivalGameMode::Logout(AController* Exit)
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	AP48PlayerState* ExitingPlayer = IsValid(Exit)
		? Exit->GetPlayerState<AP48PlayerState>() : nullptr;

	const bool bTiebreakerLogout = IsValid(GS)
		&& GS->IsTiebreaker()
		&& IsTiebreakerParticipant(ExitingPlayer);

	if (bTiebreakerLogout)
	{
		// 이미 탈락한 참가자도 재경기 목록에서 제거한다.
		ExitingPlayer->SetAlive(false);
		TiebreakerParticipants.RemoveAll(
			[ExitingPlayer](const TWeakObjectPtr<AP48PlayerState>& Participant)
			{
				return !Participant.IsValid() || Participant.Get() == ExitingPlayer;
			});

		UE_LOG(LogTemp, Log,
			TEXT("[Server] Tiebreaker logout: Player=%s, Remaining=%d"),
			*ExitingPlayer->GetPlayerName(), TiebreakerParticipants.Num());
	}
	else if (IsValid(GS)
		&& GS->MatchPhase == EP48MatchPhase::Playing
		&& IsCurrentRoundParticipant(ExitingPlayer)
		&& ExitingPlayer->IsAlive())
	{
		NotifyPlayerEliminated(ExitingPlayer);
	}

	Super::Logout(Exit);

	if (bTiebreakerLogout)
	{
		GetWorldTimerManager().SetTimer(
			TiebreakerLogoutTimerHandle,
			this,
			&ThisClass::CheckTiebreakerAfterLogout,
			FMath::Max(0.01f, RoundEndCheckDelay),
			false);
	}
}

void AP48SurvivalGameMode::CheckTiebreakerAfterLogout()
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS) || !GS->IsTiebreaker())
	{
		return;
	}

	if (GS->MatchPhase != EP48MatchPhase::RoundEnd
		&& GS->MatchPhase != EP48MatchPhase::Countdown
		&& GS->MatchPhase != EP48MatchPhase::Playing)
	{
		return;
	}

	TiebreakerParticipants.RemoveAll(
		[](const TWeakObjectPtr<AP48PlayerState>& Participant)
		{
			const AP48PlayerState* Player = Participant.Get();
			return !IsValid(Player) || !Player->IsMatchParticipant();
		});

	const int32 RemainingCount = TiebreakerParticipants.Num();
	if (RemainingCount == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Server] Tiebreaker ended: No participants remain"));
		StartMatchDraw();
		return;
	}

	if (RemainingCount == 1)
	{
		AP48PlayerState* Winner = TiebreakerParticipants[0].Get();
		UE_LOG(LogTemp, Log, TEXT("[Server] Tiebreaker forfeit winner: %s"),
			*Winner->GetPlayerName());
		StartMatchEnd(Winner);
		return;
	}

	if (GS->MatchPhase == EP48MatchPhase::Playing)
	{
		RoundEndCheck();
	}
}

bool AP48SurvivalGameMode::HasFinishedDefaultRounds() const
{
	const AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (!IsValid(P48GameState))
	{
		return false;
	}

	const int32 RequiredRounds = FMath::Max(1, DefaultRoundCount);

	return P48GameState->CurrentRound >= RequiredRounds;
}

TArray<AP48PlayerState*> AP48SurvivalGameMode::FindTopRoundWinners() const
{
	TArray<AP48PlayerState*> TopPlayers;

	const AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (!IsValid(P48GameState))
	{
		return TopPlayers;
	}

	int32 MaxWins = -1;

	for (APlayerState* PlayerState : P48GameState->PlayerArray)
	{
		AP48PlayerState* Player = Cast<AP48PlayerState>(PlayerState);

		if (!IsValid(Player) || !Player->IsMatchParticipant()) // 유효한 Match 참가자만 비교
		{
			continue;
		}

		const int32 Wins = Player->GetRoundWinCount();

		if (Wins > MaxWins)
		{
			// 더 높은 승수를 찾았으므로 기존 후보 교체
			MaxWins = Wins;
			TopPlayers.Reset();
			TopPlayers.Add(Player);
		}
		else if (Wins == MaxWins)
		{
			TopPlayers.Add(Player); // 최다 승수가 같으면 공동 후보로 추가
		}
	}

	return TopPlayers;
}

bool AP48SurvivalGameMode::TryFinishMatchWithSingleLeader()
{
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (!HasAuthority()
		|| !IsValid(P48GameState)
		|| P48GameState->MatchPhase != EP48MatchPhase::Playing)
	{
		return false;
	}

	// 일반 라운드를 모두 진행한 뒤에만 최종 승자 판정
	if (!HasFinishedDefaultRounds())
	{
		return false;
	}

	const TArray<AP48PlayerState*> TopPlayers = FindTopRoundWinners();

	// 동점이거나 참가자가 없으면 여기서는 종료하지 않음
	if (TopPlayers.Num() != 1)
	{
		return false;
	}

	AP48PlayerState* Winner = TopPlayers[0];

	UE_LOG(LogTemp, Log, TEXT("[Server] Single match leader: Player=%s, Wins=%d"),
		*Winner->GetPlayerName(),
		Winner->GetRoundWinCount());

	StartMatchEnd(Winner);
	return true;
}

bool AP48SurvivalGameMode::IsTiebreakerParticipant( const AP48PlayerState* Player) const
{
	if (!IsValid(Player))
	{
		return false;
	}

	for (const TWeakObjectPtr<AP48PlayerState>& Participant : TiebreakerParticipants)
	{
		if (Participant.Get() == Player)
		{
			return true;
		}
	}

	return false;
}

bool AP48SurvivalGameMode::IsCurrentRoundParticipant(const AP48PlayerState* Player) const
{
	if (!IsValid(Player) || !Player->IsMatchParticipant())
	{
		return false;
	}

	const AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!IsValid(GS))
	{
		return false;
	}

	return !GS->IsTiebreaker() || IsTiebreakerParticipant(Player);
}

void AP48SurvivalGameMode::StartTiebreaker(
	const TArray<AP48PlayerState*>& Participants)
{
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (!HasAuthority()
		|| !IsValid(P48GameState)
		|| P48GameState->MatchPhase != EP48MatchPhase::Playing
		|| P48GameState->IsTiebreaker())
	{
		return;
	}

	// 이전 목록을 비우고 유효한 참가자만 등록
	TiebreakerParticipants.Reset();

	for (AP48PlayerState* Player : Participants)
	{
		if (!IsValid(Player) || !Player->IsMatchParticipant())
		{
			continue;
		}

		TiebreakerParticipants.AddUnique(TWeakObjectPtr<AP48PlayerState>(Player));
	}

	if (TiebreakerParticipants.Num() < 2) // 결정전 시작에는 최소 2명이 필요
	{
		UE_LOG(LogTemp, Warning, TEXT("[Server] Cannot start tiebreaker: Participants=%d"), TiebreakerParticipants.Num());

		TiebreakerParticipants.Reset();
		return;
	}

	// 클라이언트에도 결정전 상태 전달
	P48GameState->SetTiebreaker(true);

	UE_LOG(LogTemp, Log, TEXT("[Server] Tiebreaker scheduled: Participants=%d"), TiebreakerParticipants.Num());

	for (const TWeakObjectPtr<AP48PlayerState>& Participant : TiebreakerParticipants)
	{
		if (AP48PlayerState* Player = Participant.Get())
		{
			UE_LOG(
				LogTemp,
				Log,
				TEXT("[Server] Tiebreaker participant: %s, Wins=%d"),
				*Player->GetPlayerName(),
				Player->GetRoundWinCount());
		}
	}

	// 기존 라운드 종료 흐름을 통해 결정전 준비
	StartRoundEnd();
}
