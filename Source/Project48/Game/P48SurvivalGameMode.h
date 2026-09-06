// P48SurvivalGameMode.h

#pragma once

#include "CoreMinimal.h"
#include "P48GameModeBase.h"
#include "P48SurvivalGameMode.generated.h"

class AP48PlayerState;

/**
 * @brief 생존 참가자의 탈락 처리와 라운드 승패 판정을 담당하는 서버 전용 GameMode
 *
 * 서버에서 확정된 플레이어 탈락 정보 처리 및 생존 참가자 수 계산
 * 동시 탈락 고려하여 라운드 종료를 지연 판정
 * 마지막 생존자를 라운드 승리로 처리하거나 생존자가 없을 시 무승부로 처리
 */
UCLASS()
class PROJECT48_API AP48SurvivalGameMode : public AP48GameModeBase
{
	GENERATED_BODY()

public:
	virtual void Logout(AController* Exit) override;

	// 서버에서 확정된 플레이어 탈락 정보르 전달하는 진입점
	void NotifyPlayerEliminated(AP48PlayerState* EliminatedPlayer);

	// 해당 플레이어가 결정전 참가자 목록에 있는지 확인
	bool IsTiebreakerParticipant(const AP48PlayerState* Player) const;

protected:
	// 현재 라운드에 참가할 자격이 있는 플레이어인지 확인
	bool IsCurrentRoundParticipant(const AP48PlayerState* Player) const;

	virtual void StartMatch() override;

	// 현재 생존 중인 Match 참가자 수 반환
	int32 GetAliveParticipantCount() const;

	// 현재 생존 중인 참가자 반환
	// 생존자가 한 명인 상황에만 사용!
	AP48PlayerState* LastAliveParticipant() const;

	// 라운드 종료 조건 검사
	void RoundEndCheck();

	// 생존자 수를 기준으로 라운드 종료 여부 판정
	void CheckRoundEndCondition();

	// Winner가 유효하면 승리, nullptr이면 무승부로 처리
	void FinishSurvivalRound(AP48PlayerState* Winner);

	// Match 참가자들의 생존 상태 초기화
	void ResetParticipantsForRound();

	// 일반 라운드 진행 횟수
	UPROPERTY(EditDefaultsOnly, Category = "Match", meta = (ClampMin = "1", UIMin = "1"))
	int32 DefaultRoundCount = 3;

	// 라운드 디폴트 횟수만큼 했는지 확인
	bool HasFinishedDefaultRounds() const;

	// 라운드 승수가 가장 높은 플레이어 목록 반환
	TArray<AP48PlayerState*> FindTopRoundWinners() const;

	// 단독 최다 승수 플레이어가 있으면 Match를 종료하고 true 반환
	bool TryFinishMatchWithSingleLeader();

	// 동률 플레이어를 결정전 참가자로 등록하고 다음 라운드 준비
	void StartTiebreaker(const TArray<AP48PlayerState*>& Participants);

	// Match 끝내기
	void StartMatchEnd(AP48PlayerState* Winner);

	// 승자 없이 Match 종료
	void StartMatchDraw();

	// 결정전 퇴장 시 남은 참가자 수에 따라 결과 판정
	void CheckTiebreakerAfterLogout();

private:
	// 이번 결정전에 참가할 플레이어 목록
	TArray<TWeakObjectPtr<AP48PlayerState>> TiebreakerParticipants;

	FTimerHandle RoundEndCheckTimerHandle;

	// 연속으로 발생하는 결정전 퇴장을 모아서 판정
	FTimerHandle TiebreakerLogoutTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	float RoundEndCheckDelay = 0.1f;

	// 라운드 종료 중복 검사 방지
	bool bRoundEndCheck = false;
};
