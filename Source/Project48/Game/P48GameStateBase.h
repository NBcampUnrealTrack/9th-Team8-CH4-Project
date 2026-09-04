// P48GameStateBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "P48GameStateBase.generated.h"

class AP48PlayerState;

// 델리게이트 추가
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FP48OnRoundResultChanged,
	AP48PlayerState*, Winner,
	bool, bIsDraw);

/**
 * 
 */
UENUM(BlueprintType)
enum class EP48MatchPhase : uint8
{
	Waiting,
	Countdown,
	Playing,
	RoundEnd,
	MatchEnd
};

/**
 * @brief 모든 클라이언트가 알아야 하는 공용 상태를 담당하는 클래스
 */
UCLASS()
class PROJECT48_API AP48GameStateBase : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	void SetMatchPhase(EP48MatchPhase NewMatchPhase);
	
	void SetCurrentRound(int32 NewCurrentRound);
	
	void SetRoundWinner(AP48PlayerState* NewRoundWinner);
	
	void SetRoundDraw();
	
	void ResetRoundResult();
	
	// UI에서 바인딩할 통합 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Round|Event")
	FP48OnRoundResultChanged OnRoundResultChanged;
	
	// UI에서 현재 상태를 확인할 때 사용
	UFUNCTION(BlueprintPure, Category = "Round")
	AP48PlayerState* GetRoundWinner() const
	{
		return RoundWinner;
	}

	UFUNCTION(BlueprintPure, Category = "Round")
	bool IsRoundDraw() const
	{
		return bRoundDraw;
	}

	UFUNCTION(BlueprintPure, Category = "Round")
	bool HasRoundResult() const;

	
	UPROPERTY(Replicated, BlueprintReadOnly)
	EP48MatchPhase MatchPhase = EP48MatchPhase::Waiting;
	
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Round")
	int32 CurrentRound = 0;
	
protected:
	
	// Replication 완료 후 클라이언트에서 호출
	UFUNCTION()
	void OnRep_RoundWinner();

	UFUNCTION()
	void OnRep_RoundDraw();
	
	// 서버와 클라이언트에서 라운드 결과 변경 이벤트를 전달
	void NotifyRoundResultChanged();
	
	UPROPERTY(ReplicatedUsing = OnRep_RoundWinner, BlueprintReadOnly, Category = "Round", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AP48PlayerState> RoundWinner = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_RoundDraw, BlueprintReadOnly, Category = "Round", meta = (AllowPrivateAccess = "true"))
	bool bRoundDraw = false;
};
