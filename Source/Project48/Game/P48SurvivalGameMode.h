// P48SurvivalGameMode.h

#pragma once

#include "CoreMinimal.h"
#include "P48GameModeBase.h"
#include "Rule/P48RuleResults.h"
#include "P48SurvivalGameMode.generated.h"

class AP48PlayerState;
class UP48MatchFlowRule;
class UP48RoundWinCondition;

/**
 * @brief 규칙 객체를 호출하고 생존 Match 진행을 조정하는 서버 전용 GameMode
 *
 * 서버에서 확정된 탈락과 퇴장 정보를 규칙에 전달
 * 규칙의 판정 결과에 따라 상태 변경 및 타이머 실행
 */
UCLASS()
class PROJECT48_API AP48SurvivalGameMode : public AP48GameModeBase
{
	GENERATED_BODY()

public:
	AP48SurvivalGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void Logout(AController* Exit) override;

	// 서버에서 확정된 플레이어 탈락 정보를 전달하는 진입점
	void NotifyPlayerEliminated(AP48PlayerState* EliminatedPlayer);

	// 해당 플레이어가 결정전 참가자 목록에 있는지 확인
	bool IsTiebreakerParticipant(const AP48PlayerState* Player) const;

protected:
	virtual void StartMatch() override;
	virtual bool ShouldAdvanceRound() const override;
	virtual bool ShouldCancelCountdown(int32 RemainingParticipants) const override;
	virtual void ClearMatchTimers() override;
	virtual bool IsRoundSpawnParticipant(const AP48PlayerState* Player) const override;

	// 현재 라운드에 참가할 자격이 있는 플레이어인지 확인
	bool IsCurrentRoundParticipant(const AP48PlayerState* Player) const;

	// Match 진행에 사용할 규칙 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Match|Rule")
	TSubclassOf<UP48MatchFlowRule> MatchFlowRuleClass;

	// 현재 라운드의 승패를 판정할 규칙 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Match|Rule")
	TSubclassOf<UP48RoundWinCondition> RoundWinConditionClass;

	// 기존 Blueprint의 설정값을 유지하고 실제 진행 횟수 판정은 규칙에 위임
	UPROPERTY(EditDefaultsOnly, Category = "Match", meta = (ClampMin = "1", UIMin = "1"))
	int32 DefaultRoundCount = 3;

	// 일반 라운드와 결정전에 공통으로 적용할 제한 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Round", meta = (ClampMin = "1.0"))
	float RoundTimeLimit = 10.0f;

	FTimerHandle RoundTimeLimitTimerHandle;

	void HandleRoundTimeExpired();

private:
	// 참가 자격을 확인한 목록으로 라운드 승패 판정
	FP48RoundResult EvaluateRound() const;

	// Match 참가자들의 생존 상태 초기화
	void ResetParticipantsForRound();

	// 연속 탈락을 모아서 라운드 종료 조건 검사
	void RoundEndCheck();
	void CheckRoundEndCondition();

	// 연속 퇴장을 모은 뒤 진행 규칙으로 결과 판정
	void CheckAfterLogout();

	// 규칙이 반환한 결과에 따라 상태 전환 및 타이머 실행
	void ApplyFlowResult(const FP48MatchFlowResult& Result);

	// Winner가 유효하면 승리, nullptr이면 무승부로 Match 종료
	void FinishMatch(AP48PlayerState* Winner);

	// 서버에서 생성한 규칙 객체 보관
	UPROPERTY(Transient)
	TObjectPtr<UP48MatchFlowRule> MatchFlowRule;

	UPROPERTY(Transient)
	TObjectPtr<UP48RoundWinCondition> RoundWinCondition;

	FTimerHandle RoundEndCheckTimerHandle;
	FTimerHandle LogoutCheckTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	float RoundEndCheckDelay = 0.1f;

	// 라운드 종료 중복 검사 방지
	bool bRoundEndCheck = false;
};
