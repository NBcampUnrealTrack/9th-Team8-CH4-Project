// P48RuleResults.h

#pragma once

#include "CoreMinimal.h"

class AP48PlayerState;

// 현재 라운드의 승패 판정 결과
enum class EP48RoundOutcome : uint8
{
	InProgress,
	Winner,
	Draw
};

// 서버에서 판정 직후 사용하는 결과이며 복제 상태는 GameState에 반영
struct FP48RoundResult
{
	EP48RoundOutcome Outcome = EP48RoundOutcome::InProgress;
	AP48PlayerState* Winner = nullptr;
	int32 AliveCount = 0;
};

// 판정 결과에 따라 GameMode가 실행할 다음 진행
enum class EP48MatchFlowAction : uint8
{
	None,
	NextRound,
	StartTiebreaker,
	MatchWinner,
	MatchDraw,
	CheckRound
};

struct FP48MatchFlowResult
{
	EP48MatchFlowAction Action = EP48MatchFlowAction::None;
	AP48PlayerState* Winner = nullptr;
	bool bAwardRoundWin = false;
};
