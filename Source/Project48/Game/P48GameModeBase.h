// P48GameModeBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "P48GameModeBase.generated.h"

/**
 * @brief 서버 전용 판정과 게임 진행을 담당하는 클래스
 */
UCLASS()
class PROJECT48_API AP48GameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void OnPostLogin(AController* NewPlayer) override;
	virtual void Logout(AController* Exit) override;
	
	void NotifyPlayerReadyStateChanged();
	void NotifyMapGenerationReadinessChanged();
	
protected:
	
	/* 게임 시작에 필요한 최소 인원 */
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	int32 MinPlayersToStart = 2;
	
	/* 카운트 다운 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	float CountdownDuration = 3.0f;
	
	/* 라운드 종료 상태 유지 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	float RoundEndDuration = 3.0f;
	
	FTimerHandle CountdownTimerHandle;
	FTimerHandle RoundEndTimerHandle;
	TArray<TWeakObjectPtr<APlayerController>> PlayersWaitingForMap;
	
	void CheckStartCondition();
	bool IsMapReadyForPlayer(const APlayerController* PlayerController) const;
	void SpawnPlayersWaitingForMap();
	bool AreAllPlayersReady() const;
	void ConfirmMatchParticipants();
	
	void StartCountdown();
	virtual void StartMatch();
	void StartRoundEnd();
	virtual void PrepareNextRound();

	// 다음 라운드 준비 시 일반 라운드 번호 증가 여부
	virtual bool ShouldAdvanceRound() const { return true; }

	// 참가자 퇴장 시 카운트다운 취소 여부
	virtual bool ShouldCancelCountdown(int32 RemainingParticipants) const;

	// Match 종료 및 월드 종료 시 진행 타이머 정리
	virtual void ClearMatchTimers();
};
