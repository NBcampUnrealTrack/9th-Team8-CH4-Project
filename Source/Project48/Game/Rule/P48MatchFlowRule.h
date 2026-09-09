// P48MatchFlowRule.h

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "P48RuleResults.h"
#include "P48MatchFlowRule.generated.h"

class AP48GameStateBase;

/**
 * @brief 라운드 결과와 참가자 변동에 따른 Match 진행 규칙 공통 기반
 *
 * 서버 GameMode에서 생성하여 사용
 * GameState와 PlayerState는 조회만 하고 변경 사항은 결과로 전달
 */
UCLASS(Abstract)
class PROJECT48_API UP48MatchFlowRule : public UObject
{
	GENERATED_BODY()

public:
	// 기존 GameMode에 저장된 일반 라운드 설정 전달
	virtual void Initialize(int32 InDefaultRoundCount) {}

	// 현재 라운드에 참가할 자격이 있는 플레이어인지 확인
	virtual bool IsRoundParticipant(const AP48GameStateBase& State, const AP48PlayerState* Player) const
		PURE_VIRTUAL(UP48MatchFlowRule::IsRoundParticipant, return false;);

	// 라운드 결과를 받아 승수 반영 여부와 다음 진행 반환
	virtual FP48MatchFlowResult ResolveRound(const AP48GameStateBase& State, const FP48RoundResult& Round)
		PURE_VIRTUAL(UP48MatchFlowRule::ResolveRound, return FP48MatchFlowResult(););

	// 퇴장자를 명단에서 제거하고 지연 판정이 필요하면 true 반환
	virtual bool RemoveParticipant(const AP48GameStateBase& State, AP48PlayerState* Player) { return false; }

	// 연속 퇴장을 모은 뒤 남은 참가자를 기준으로 판정
	virtual FP48MatchFlowResult ResolveLogout(const AP48GameStateBase& State) { return {}; }

	// 다음 라운드 준비 시 일반 라운드 번호 증가 여부
	virtual bool ShouldAdvanceRound(const AP48GameStateBase& State) const { return true; }

	// 참가자 퇴장 시 일반 카운트다운 취소 여부
	virtual bool ShouldCancelCountdown(const AP48GameStateBase& State, int32 Remaining, int32 Minimum) const
	{
		return Remaining < Minimum;
	}

	// 기존 GameMode의 결정전 참가자 조회 진입점 유지
	virtual bool IsTiebreakerParticipant(const AP48PlayerState* Player) const { return false; }

	// Match 종료 시 규칙이 보유한 참가자 정보 정리
	virtual void Reset() {}
};
