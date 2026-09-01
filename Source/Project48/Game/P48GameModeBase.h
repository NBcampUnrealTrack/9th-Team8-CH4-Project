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
	virtual void OnPostLogin(AController* NewPlayer) override;
	virtual void Logout(AController* Exit) override;
	
	void NotifyPlayerReadyStateChanged();
	
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
	
	void CheckStartCondition();
	bool AreAllPlayersReady() const;
	void ConfirmMatchParticipants();
	
	void StartCountdown();
	void StartMatch();
	void StartRoundEnd();
	virtual void PrepareNextRound();
};
