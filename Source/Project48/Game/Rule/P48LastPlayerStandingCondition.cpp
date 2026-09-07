// P48LastPlayerStandingCondition.cpp

#include "P48LastPlayerStandingCondition.h"

#include "Project48/Character/P48PlayerState.h"

FP48RoundResult UP48LastPlayerStandingCondition::Evaluate(
	const TArray<AP48PlayerState*>& Participants) const
{
	FP48RoundResult Result;

	// 한 번의 순회에서 생존자 수와 마지막 생존자를 함께 확인
	for (AP48PlayerState* Player : Participants)
	{
		if (IsValid(Player) && Player->IsAlive())
		{
			Result.AliveCount++;
			Result.Winner = Player;
		}
	}

	if (Result.AliveCount == 1)
	{
		Result.Outcome = EP48RoundOutcome::Winner;
	}
	else
	{
		Result.Winner = nullptr;
		Result.Outcome = Result.AliveCount == 0
			? EP48RoundOutcome::Draw : EP48RoundOutcome::InProgress;
	}

	return Result;
}
