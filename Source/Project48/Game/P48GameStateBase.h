// P48GameStateBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Project48/UI/P48RankingData.h"
#include "P48GameStateBase.generated.h"

class AP48PlayerState;

struct FChatMessage;

// 델리게이트 추가
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FP48OnRoundResultChanged,
	AP48PlayerState*, Winner,
	bool, bIsDraw);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FP48OnCurrentRoundChanged,
	int32,
	NewRound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(
	FP48OnMatchEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(
	FP48OnFinalRankingDataReady);
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

	// 서버가 결과 표시 대기 후 각 클라이언트의 로비 복귀를 요청한다.
	void RequestLobbyReturn();
	
	void SetCurrentRound(int32 NewCurrentRound);
	
	void SetRoundWinner(AP48PlayerState* NewRoundWinner);
	
	void SetRoundDraw();
	
	void ResetRoundResult();
	
	void SetMatchWinner(AP48PlayerState* NewMatchWinner);
	
	void ResetMatchResult();
	
	// 서버에서 결정전 진행 상태 변경
	void SetTiebreaker(bool bNewTiebreaker);

	// 서버 기준 라운드 종료 시각 설정, 0이면 시간제한 비활성
	void SetRoundEndServerTime(double NewEndTime);

	// 서버와 동기화된 시각으로 UI에 표시할 남은 시간 계산
	UFUNCTION(BlueprintPure, Category = "Round")
	float GetRemainingRoundTime() const;

	// 서버에서 라운드 규칙에 전달할 시간 만료 여부 조회
	bool IsRoundTimeExpired() const;

	// 현재 결정전 상태인지 조회
	UFUNCTION(BlueprintPure, Category = "Match")
	bool IsTiebreaker() const
	{
		return bIsTiebreaker;
	}
	
	// UI에서 바인딩할 통합 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Round|Event")
	FP48OnRoundResultChanged OnRoundResultChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Round|Event")
	FP48OnCurrentRoundChanged OnCurrentRoundChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Match|Event")
	FP48OnMatchEnded OnMatchEnded;
	
	UPROPERTY(BlueprintAssignable, Category = "Match|Event")
	FP48OnFinalRankingDataReady OnFinalRankingDataReady;
	
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
	
	UFUNCTION(BlueprintPure, Category = "Match")
	AP48PlayerState* GetMatchWinner() const
	{
		return MatchWinner;
	}

	UFUNCTION(BlueprintPure, Category = "Round")
	bool HasRoundResult() const;

	
	//UPROPERTY(Replicated, BlueprintReadOnly)
	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase, BlueprintReadOnly)
	EP48MatchPhase MatchPhase = EP48MatchPhase::Waiting;
	
	UPROPERTY(ReplicatedUsing = OnRep_CurrentRound, BlueprintReadOnly, Category = "Round")
	int32 CurrentRound = 0;
	
protected:
	UPROPERTY(ReplicatedUsing = OnRep_LobbyReturnRequested)
	bool bLobbyReturnRequested = false;

	UFUNCTION()
	void OnRep_LobbyReturnRequested();

	// 매초 남은 시간을 복제하는 대신 종료 시각만 공유
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Round")
	double RoundEndServerTime = 0.0;
	
	// Replication 완료 후 클라이언트에서 호출
	UFUNCTION()
	void OnRep_RoundWinner();

	UFUNCTION()
	void OnRep_RoundDraw();
	
	UFUNCTION()
	void OnRep_MatchWinner();
	
	UFUNCTION()
	void OnRep_CurrentRound();
	
	UFUNCTION()
	void OnRep_MatchPhase();
	
	// 서버와 클라이언트에서 라운드 결과 변경 이벤트를 전달
	void NotifyRoundResultChanged();
	
	UPROPERTY(ReplicatedUsing = OnRep_RoundWinner, BlueprintReadOnly, Category = "Round", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AP48PlayerState> RoundWinner = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_RoundDraw, BlueprintReadOnly, Category = "Round", meta = (AllowPrivateAccess = "true"))
	bool bRoundDraw = false;
	
	UPROPERTY(ReplicatedUsing = OnRep_MatchWinner, BlueprintReadOnly, Category = "Match")
	TObjectPtr<AP48PlayerState> MatchWinner = nullptr;
	
	// 결정전 준비 및 진행 중인지 여부
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	bool bIsTiebreaker = false;
	
	/* UI */
public:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastReceiveChatMessage(const FChatMessage& InChatMessage);
	
public:
	// 매치 종료 시 서버가 확정한 최종 랭킹 데이터를 생성
	void BuildFinalRankingData();

	// 클라이언트에서 최종 랭킹 데이터가 준비되었는지 확인
	bool HasFinalRankingData() const
	{
		return bFinalRankingDataReady;
	}

	// 최종 랭킹 데이터 가져오기
	const TArray<FP48RankingData>& GetFinalRankingData() const
	{
		return FinalRankingData;
	}
	
protected:
	// 서버가 매치 종료 직전에 확정한 최종 랭킹
	UPROPERTY(ReplicatedUsing = OnRep_FinalRankingData, BlueprintReadOnly, Category = "Match")
	TArray<FP48RankingData> FinalRankingData;

	// 최종 랭킹 데이터 복제가 완료되었는지
	UPROPERTY(ReplicatedUsing = OnRep_FinalRankingDataReady, BlueprintReadOnly, Category = "Match")
	bool bFinalRankingDataReady = false;

	UFUNCTION()
	void OnRep_FinalRankingData();

	UFUNCTION()
	void OnRep_FinalRankingDataReady();
};
