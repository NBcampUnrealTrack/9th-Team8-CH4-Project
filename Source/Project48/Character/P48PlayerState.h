// P48PlayerState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "P48PlayerState.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT48_API AP48PlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	bool IsReady() const {return bIsReady; }
	bool IsAlive() const {return bIsAlive; }
	bool IsMatchParticipant() const { return bIsMatchParticipant; }
	int32 GetRoundWinCount() const { return RoundWinCount; }
	int32 GetStunCount() const {return StunCount; }

protected:
	// 플레이어가 현재 Match 시작 준비를 완료했는지
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	bool bIsReady = false;

	// 플레이어가 현재 라운드에서 생존 중인지
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Match")
	bool bIsAlive = false;

	// 현재 진행 중인 Match의 정식 참가자인지 (중도 참가자가 아닌지 확인)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	bool bIsMatchParticipant = false;

	// 현재 Match에서 해당 플레이어가 승리한 라운드의 수
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	int32 RoundWinCount = 0;
	
	// StunCount
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Stun")
	int32 StunCount = 0;
	
public:
	void SetReady(bool bNewReady);
	void SetAlive(bool bNewAlive);
	void SetMatchParticipant(bool bNewParticipant);
	void AddRoundWin();
	void ResetForNewRound();
	void ResetForNewMatch();
	
	// 서버 전용 스턴 카운트 증가/리셋 함수
	void AddStunCount(int32 Amount = 1);
	void ResetStunCount();
	
	// 사망 처리 함수 (스턴 누적, 낙사 등)
	void OnDeath();
};
