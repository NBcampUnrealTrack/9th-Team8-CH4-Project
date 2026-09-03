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
	
protected:
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
	
private:
	FTimerHandle RoundEndCheckTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	float RoundEndCheckDelay = 0.1f;
	
	// 라운드 종료 중복 검사 방지
	bool bRoundEndCheck = false;
};
