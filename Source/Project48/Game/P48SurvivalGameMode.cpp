// P48SurvivalGameMode.cpp

#include "P48SurvivalGameMode.h"

#include "P48GameStateBase.h"
#include "Project48/Character/P48PlayerState.h"
#include "Rule/P48BestOfRoundsMatchFlowRule.h"
#include "Rule/P48LastPlayerStandingCondition.h"
#include "TimerManager.h"

AP48SurvivalGameMode::AP48SurvivalGameMode()
{
	MatchFlowRuleClass = UP48BestOfRoundsMatchFlowRule::StaticClass();
	RoundWinConditionClass = UP48LastPlayerStandingCondition::StaticClass();
}

void AP48SurvivalGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (!HasAuthority())
	{
		return;
	}

	// 플레이어 접속과 카운트다운 전에 규칙 객체 생성
	UClass* FlowClass = MatchFlowRuleClass.Get();
	UClass* ConditionClass = RoundWinConditionClass.Get();
	if (!FlowClass || FlowClass->HasAnyClassFlags(CLASS_Abstract))
	{
		FlowClass = UP48BestOfRoundsMatchFlowRule::StaticClass();
	}
	if (!ConditionClass || ConditionClass->HasAnyClassFlags(CLASS_Abstract))
	{
		ConditionClass = UP48LastPlayerStandingCondition::StaticClass();
	}

	MatchFlowRule = NewObject<UP48MatchFlowRule>(this, FlowClass);
	RoundWinCondition = NewObject<UP48RoundWinCondition>(this, ConditionClass);
	MatchFlowRule->Initialize(DefaultRoundCount);
}

bool AP48SurvivalGameMode::IsCurrentRoundParticipant(const AP48PlayerState* Player) const
{
	const AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	return IsValid(GS) && IsValid(MatchFlowRule) && MatchFlowRule->IsRoundParticipant(*GS, Player);
}

bool AP48SurvivalGameMode::IsTiebreakerParticipant(const AP48PlayerState* Player) const
{
	return IsValid(MatchFlowRule) && MatchFlowRule->IsTiebreakerParticipant(Player);
}

FP48RoundResult AP48SurvivalGameMode::EvaluateRound() const
{
	const AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!IsValid(GS) || !IsValid(MatchFlowRule) || !IsValid(RoundWinCondition))
	{
		return {};
	}

	TArray<AP48PlayerState*> Participants;
	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		AP48PlayerState* Player = Cast<AP48PlayerState>(PlayerState);
		if (MatchFlowRule->IsRoundParticipant(*GS, Player))
		{
			Participants.Add(Player);
		}
	}

	return RoundWinCondition->Evaluate(Participants);
}

void AP48SurvivalGameMode::NotifyPlayerEliminated(AP48PlayerState* EliminatedPlayer)
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS) || GS->MatchPhase != EP48MatchPhase::Playing
		|| !IsCurrentRoundParticipant(EliminatedPlayer) || !EliminatedPlayer->IsAlive())
	{
		return;
	}

	EliminatedPlayer->SetAlive(false);
	UE_LOG(LogTemp, Warning, TEXT("[Server] Player eliminated: %s, Alive participants: %d"),
		*EliminatedPlayer->GetPlayerName(), EvaluateRound().AliveCount);

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
		RoundEndCheckTimerHandle, this, &ThisClass::CheckRoundEndCondition, RoundEndCheckDelay, false);
}

void AP48SurvivalGameMode::CheckRoundEndCondition()
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS) || !IsValid(MatchFlowRule)
		|| GS->MatchPhase != EP48MatchPhase::Playing)
	{
		bRoundEndCheck = false;
		return;
	}

	// 연속 퇴장을 모으는 동안 기존 사망 판정이 먼저 승자를 확정하지 않도록 한다.
	if (GetWorldTimerManager().IsTimerActive(LogoutCheckTimerHandle))
	{
		bRoundEndCheck = false;
		return;
	}

	const FP48RoundResult Round = EvaluateRound();
	UE_LOG(LogTemp, Warning, TEXT("[Server] Round end check: Alive participants = %d"), Round.AliveCount);
	if (Round.Outcome == EP48RoundOutcome::InProgress)
	{
		bRoundEndCheck = false;
		return;
	}

	const FP48MatchFlowResult Flow = MatchFlowRule->ResolveRound(*GS, Round);
	if (Flow.Action == EP48MatchFlowAction::None)
	{
		bRoundEndCheck = false;
		return;
	}

	// 복제 상태 변경과 승수 반영은 서버 GameMode에서 처리
	if (Round.Outcome == EP48RoundOutcome::Winner)
	{
		GS->SetRoundWinner(Round.Winner);
		if (Flow.bAwardRoundWin)
		{
			Round.Winner->AddRoundWin();
		}
	}
	else
	{
		GS->SetRoundDraw();
	}

	ApplyFlowResult(Flow);
}

void AP48SurvivalGameMode::ResetParticipantsForRound()
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!IsValid(GS))
	{
		return;
	}

	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		AP48PlayerState* Player = Cast<AP48PlayerState>(PlayerState);
		if (IsValid(Player))
		{
			Player->ResetForNewRound();
			if (!IsCurrentRoundParticipant(Player))
			{
				Player->SetAlive(false);
			}
		}
	}
}

void AP48SurvivalGameMode::StartMatch()
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS) || !IsValid(MatchFlowRule) || !IsValid(RoundWinCondition)
		|| GS->MatchPhase != EP48MatchPhase::Countdown)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RoundEndCheckTimerHandle);
	bRoundEndCheck = false;
	GS->ResetRoundResult();
	ResetParticipantsForRound();
	Super::StartMatch();

	UE_LOG(LogTemp, Warning, TEXT("[Server] Survival round started: Alive participants = %d"), EvaluateRound().AliveCount);
}

void AP48SurvivalGameMode::Logout(AController* Exit)
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	AP48PlayerState* Player = IsValid(Exit) ? Exit->GetPlayerState<AP48PlayerState>() : nullptr;

	// Super::Logout 호출 전에 퇴장자를 규칙의 참가자 목록에서 제거
	const bool bCheckLogout = HasAuthority() && IsValid(GS) && IsValid(MatchFlowRule)
		&& MatchFlowRule->RemoveParticipant(*GS, Player);
	if (bCheckLogout)
	{
		Player->SetAlive(false);
	}
	else if (IsValid(GS) && GS->MatchPhase == EP48MatchPhase::Playing)
	{
		NotifyPlayerEliminated(Player);
	}

	Super::Logout(Exit);

	if (bCheckLogout)
	{
		GetWorldTimerManager().SetTimer(
			LogoutCheckTimerHandle, this, &ThisClass::CheckAfterLogout,
			FMath::Max(0.01f, RoundEndCheckDelay), false);
	}
}

void AP48SurvivalGameMode::CheckAfterLogout()
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS) || !IsValid(MatchFlowRule))
	{
		return;
	}

	ApplyFlowResult(MatchFlowRule->ResolveLogout(*GS));
}

void AP48SurvivalGameMode::ApplyFlowResult(const FP48MatchFlowResult& Result)
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS))
	{
		return;
	}

	switch (Result.Action)
	{
	case EP48MatchFlowAction::StartTiebreaker:
		GS->SetTiebreaker(true);
		UE_LOG(LogTemp, Log, TEXT("[Server] Tiebreaker scheduled"));
		StartRoundEnd();
		break;
	case EP48MatchFlowAction::ReplayTiebreaker:
		UE_LOG(LogTemp, Log, TEXT("[Server] Tiebreaker draw: Scheduling replay"));
		StartRoundEnd();
		break;
	case EP48MatchFlowAction::NextRound:
		StartRoundEnd();
		break;
	case EP48MatchFlowAction::MatchWinner:
		if (IsValid(Result.Winner))
		{
			FinishMatch(Result.Winner);
		}
		break;
	case EP48MatchFlowAction::MatchDraw:
		FinishMatch(nullptr);
		break;
	case EP48MatchFlowAction::CheckRound:
		RoundEndCheck();
		break;
	case EP48MatchFlowAction::None:
		break;
	}
}

void AP48SurvivalGameMode::FinishMatch(AP48PlayerState* Winner)
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS)
		|| GS->MatchPhase == EP48MatchPhase::Waiting || GS->MatchPhase == EP48MatchPhase::MatchEnd)
	{
		return;
	}

	// 승리와 무승부 모두 같은 경로에서 타이머 및 규칙 정보 정리
	ClearMatchTimers();
	if (IsValid(Winner))
	{
		GS->SetMatchWinner(Winner);
	}
	else
	{
		GS->ResetMatchResult();
		UE_LOG(LogTemp, Log, TEXT("[Server] Match ended in a draw"));
	}

	GS->SetTiebreaker(false);
	if (IsValid(MatchFlowRule))
	{
		MatchFlowRule->Reset();
	}
	GS->SetMatchPhase(EP48MatchPhase::MatchEnd);
}

void AP48SurvivalGameMode::ClearMatchTimers()
{
	Super::ClearMatchTimers();
	GetWorldTimerManager().ClearTimer(RoundEndCheckTimerHandle);
	GetWorldTimerManager().ClearTimer(LogoutCheckTimerHandle);
	bRoundEndCheck = false;
}

bool AP48SurvivalGameMode::ShouldAdvanceRound() const
{
	const AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	return IsValid(GS) && IsValid(MatchFlowRule) && MatchFlowRule->ShouldAdvanceRound(*GS);
}

bool AP48SurvivalGameMode::ShouldCancelCountdown(int32 RemainingParticipants) const
{
	const AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	return IsValid(GS) && IsValid(MatchFlowRule)
		&& MatchFlowRule->ShouldCancelCountdown(*GS, RemainingParticipants, MinPlayersToStart);
}
