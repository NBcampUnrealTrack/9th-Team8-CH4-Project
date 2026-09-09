// P48BestOfRoundsMatchFlowRule.cpp

#include "P48BestOfRoundsMatchFlowRule.h"

#include "Project48/Character/P48PlayerState.h"
#include "Project48/Game/P48GameStateBase.h"

void UP48BestOfRoundsMatchFlowRule::Initialize(int32 InDefaultRoundCount)
{
	DefaultRoundCount = FMath::Max(1, InDefaultRoundCount);
	Reset();
}

bool UP48BestOfRoundsMatchFlowRule::IsRoundParticipant(
	const AP48GameStateBase& State, const AP48PlayerState* Player) const
{
	return IsValid(Player) && Player->IsMatchParticipant()
		&& (!State.IsTiebreaker() || IsTiebreakerParticipant(Player));
}

FP48MatchFlowResult UP48BestOfRoundsMatchFlowRule::ResolveRound(
	const AP48GameStateBase& State, const FP48RoundResult& Round)
{
	FP48MatchFlowResult Result;
	if (State.MatchPhase != EP48MatchPhase::Playing || Round.Outcome == EP48RoundOutcome::InProgress)
	{
		return Result;
	}

	AP48PlayerState* Winner = Round.Outcome == EP48RoundOutcome::Winner ? Round.Winner : nullptr;
	if (Round.Outcome == EP48RoundOutcome::Winner && !IsRoundParticipant(State, Winner))
	{
		return Result;
	}

	// 결정전 승리도 승수에 반영하고 누적 승수와 관계없이 Match 종료
	Result.bAwardRoundWin = Winner != nullptr;
	if (State.IsTiebreaker())
	{
		// 결정전 무승부는 재경기 없이 Match 무승부로 종료
		Result.Action = Winner ? EP48MatchFlowAction::MatchWinner : EP48MatchFlowAction::MatchDraw;
		Result.Winner = Winner;
		return Result;
	}

	// 실제 승수 반영 전에 이번 승리까지 포함하여 2승 달성 여부 확인
	if (Winner && Winner->GetRoundWinCount() + 1 >= 2)
	{
		Result.Action = EP48MatchFlowAction::MatchWinner;
		Result.Winner = Winner;
		return Result;
	}

	if (State.CurrentRound < DefaultRoundCount)
	{
		Result.Action = EP48MatchFlowAction::NextRound;
		return Result;
	}

	// 실제 승수 변경은 GameMode에서 수행하므로 이번 승리까지 미리 계산
	const TArray<AP48PlayerState*> TopPlayers = FindTopRoundWinners(State, Winner);
	if (TopPlayers.Num() == 1)
	{
		Result.Action = EP48MatchFlowAction::MatchWinner;
		Result.Winner = TopPlayers[0];
	}
	else if (TopPlayers.Num() > 1)
	{
		TiebreakerParticipants.Reset();
		for (AP48PlayerState* Player : TopPlayers)
		{
			TiebreakerParticipants.AddUnique(TWeakObjectPtr<AP48PlayerState>(Player));
		}
		Result.Action = EP48MatchFlowAction::StartTiebreaker;
	}
	else
	{
		Result.Action = EP48MatchFlowAction::MatchDraw;
	}

	return Result;
}

TArray<AP48PlayerState*> UP48BestOfRoundsMatchFlowRule::FindTopRoundWinners(
	const AP48GameStateBase& State, AP48PlayerState* PendingWinner) const
{
	TArray<AP48PlayerState*> TopPlayers;
	int32 MaxWins = -1;

	for (APlayerState* PlayerState : State.PlayerArray)
	{
		AP48PlayerState* Player = Cast<AP48PlayerState>(PlayerState);
		if (!IsValid(Player) || !Player->IsMatchParticipant())
		{
			continue;
		}

		const int32 Wins = Player->GetRoundWinCount() + (Player == PendingWinner ? 1 : 0);
		if (Wins > MaxWins)
		{
			// 더 높은 승수를 찾았으므로 기존 후보 교체
			MaxWins = Wins;
			TopPlayers.Reset();
			TopPlayers.Add(Player);
		}
		else if (Wins == MaxWins)
		{
			TopPlayers.Add(Player);
		}
	}

	return TopPlayers;
}

bool UP48BestOfRoundsMatchFlowRule::RemoveParticipant(
	const AP48GameStateBase& State, AP48PlayerState* Player)
{
	if (!State.IsTiebreaker() || !IsTiebreakerParticipant(Player))
	{
		return false;
	}

	// 이미 탈락한 참가자도 결정전 참가자 목록에서 제거
	TiebreakerParticipants.RemoveAll(
		[Player](const TWeakObjectPtr<AP48PlayerState>& Participant)
		{
			return !Participant.IsValid() || Participant.Get() == Player;
		});

	UE_LOG(LogTemp, Log, TEXT("[Server] Tiebreaker logout: Player=%s, Remaining=%d"),
		*Player->GetPlayerName(), TiebreakerParticipants.Num());
	return true;
}

FP48MatchFlowResult UP48BestOfRoundsMatchFlowRule::ResolveLogout(const AP48GameStateBase& State)
{
	FP48MatchFlowResult Result;
	if (!State.IsTiebreaker()
		|| (State.MatchPhase != EP48MatchPhase::RoundEnd
			&& State.MatchPhase != EP48MatchPhase::Countdown
			&& State.MatchPhase != EP48MatchPhase::Playing))
	{
		return Result;
	}

	TiebreakerParticipants.RemoveAll(
		[](const TWeakObjectPtr<AP48PlayerState>& Participant)
		{
			return !Participant.IsValid() || !Participant->IsMatchParticipant();
		});

	if (TiebreakerParticipants.Num() == 0)
	{
		Result.Action = EP48MatchFlowAction::MatchDraw;
	}
	else if (TiebreakerParticipants.Num() == 1)
	{
		// 부전승은 생존 여부와 관계없이 남은 결정전 참가자를 기준으로 판정
		Result.Action = EP48MatchFlowAction::MatchWinner;
		Result.Winner = TiebreakerParticipants[0].Get();
	}
	else if (State.MatchPhase == EP48MatchPhase::Playing)
	{
		Result.Action = EP48MatchFlowAction::CheckRound;
	}

	return Result;
}

bool UP48BestOfRoundsMatchFlowRule::ShouldAdvanceRound(const AP48GameStateBase& State) const
{
	return !State.IsTiebreaker();
}

bool UP48BestOfRoundsMatchFlowRule::ShouldCancelCountdown(
	const AP48GameStateBase& State, int32 Remaining, int32 Minimum) const
{
	return !State.IsTiebreaker() && Remaining < Minimum;
}

bool UP48BestOfRoundsMatchFlowRule::IsTiebreakerParticipant(const AP48PlayerState* Player) const
{
	if (!IsValid(Player))
	{
		return false;
	}

	return TiebreakerParticipants.ContainsByPredicate(
		[Player](const TWeakObjectPtr<AP48PlayerState>& Participant)
		{
			return Participant.Get() == Player;
		});
}

void UP48BestOfRoundsMatchFlowRule::Reset()
{
	TiebreakerParticipants.Reset();
}
