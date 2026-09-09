// P48BestOfRoundsMatchFlowRule.h

#pragma once

#include "CoreMinimal.h"
#include "P48MatchFlowRule.h"
#include "P48BestOfRoundsMatchFlowRule.generated.h"

/**
 * @brief 2승 달성 시 즉시 종료하고, 미달성 시 최다 승수와 결정전으로 승자를 정하는 규칙
 *
 * 일반 라운드 무승부도 진행 횟수에 포함
 * 결정전은 일반 라운드 번호에 포함하지 않으며 승리는 승수에 반영, 무승부는 Match 종료
 */
UCLASS()
class PROJECT48_API UP48BestOfRoundsMatchFlowRule : public UP48MatchFlowRule
{
	GENERATED_BODY()

public:
	virtual void Initialize(int32 InDefaultRoundCount) override;
	virtual bool IsRoundParticipant(const AP48GameStateBase& State, const AP48PlayerState* Player) const override;
	virtual FP48MatchFlowResult ResolveRound(const AP48GameStateBase& State, const FP48RoundResult& Round) override;
	virtual bool RemoveParticipant(const AP48GameStateBase& State, AP48PlayerState* Player) override;
	virtual FP48MatchFlowResult ResolveLogout(const AP48GameStateBase& State) override;
	virtual bool ShouldAdvanceRound(const AP48GameStateBase& State) const override;
	virtual bool ShouldCancelCountdown(const AP48GameStateBase& State, int32 Remaining, int32 Minimum) const override;
	virtual bool IsTiebreakerParticipant(const AP48PlayerState* Player) const override;
	virtual void Reset() override;

private:
	// 이번 라운드 승수 증가분까지 포함한 최다 승수 플레이어 목록 반환
	TArray<AP48PlayerState*> FindTopRoundWinners(const AP48GameStateBase& State, AP48PlayerState* PendingWinner) const;

	// 일반 라운드 진행 횟수
	int32 DefaultRoundCount = 3;

	// 이번 결정전에 참가할 플레이어 목록
	TArray<TWeakObjectPtr<AP48PlayerState>> TiebreakerParticipants;
};
