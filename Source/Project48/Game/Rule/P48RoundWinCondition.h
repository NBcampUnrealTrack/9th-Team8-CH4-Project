// P48RoundWinCondition.h

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "P48RuleResults.h"
#include "P48RoundWinCondition.generated.h"

/**
 * @brief 현재 라운드 참가자의 승패를 판정하는 규칙 공통 기반
 *
 * 참가 자격이 확인된 목록을 받아 결과만 반환
 * 상태 변경과 타이머 실행은 GameMode에서 처리
 */
UCLASS(Abstract)
class PROJECT48_API UP48RoundWinCondition : public UObject
{
	GENERATED_BODY()

public:
	// 현재 라운드의 진행 중, 승리, 무승부 여부 판정
	virtual FP48RoundResult Evaluate(const TArray<AP48PlayerState*>& Participants) const
		PURE_VIRTUAL(UP48RoundWinCondition::Evaluate, return FP48RoundResult(););
};
