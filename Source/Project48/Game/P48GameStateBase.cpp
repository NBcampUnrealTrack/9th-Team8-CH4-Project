// P48GameStateBase.cpp


#include "P48GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Project48/Character/P48PlayerState.h"

void AP48GameStateBase::BeginPlay()
{
	Super::BeginPlay();

	// GameState 확인용 로그 출력
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[%s] P48GameStateBase, MatchPhase: %s"),
		HasAuthority() ? TEXT("Server") : TEXT("Client"),
		*UEnum::GetValueAsString(MatchPhase));
}

void AP48GameStateBase::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AP48GameStateBase, MatchPhase);
	DOREPLIFETIME(AP48GameStateBase, CurrentRound);
	DOREPLIFETIME(AP48GameStateBase, RoundWinner);
	DOREPLIFETIME(AP48GameStateBase, bRoundDraw);
}

void AP48GameStateBase::SetMatchPhase(EP48MatchPhase NewMatchPhase)
{
	if (HasAuthority() == false)
	{
		return;
	}
	
	MatchPhase = NewMatchPhase;
}

void AP48GameStateBase::SetCurrentRound(int32 NewCurrentRound)
{
	if (HasAuthority() == false)
	{
		return;
	}
	
	if (CurrentRound == NewCurrentRound)
	{
		return;
	}
	
	CurrentRound = NewCurrentRound;
	
	UE_LOG(LogTemp, Warning, TEXT("[Server] CurrentRound: %d"), CurrentRound);
}

void AP48GameStateBase::SetRoundWinner(AP48PlayerState* NewRoundWinner)
{
	if (HasAuthority() == false || IsValid(NewRoundWinner) == false)
	{
		return;
	}
	
	if (RoundWinner == NewRoundWinner && bRoundDraw == false)
	{
		return;
	}
	
	RoundWinner = NewRoundWinner;
	bRoundDraw = false;
	
	UE_LOG(LogTemp, Warning, TEXT("[Server] < Round result > Round Winner = %s"), *NewRoundWinner->GetPlayerName());
	
	NotifyRoundResultChanged();
}

void AP48GameStateBase::SetRoundDraw()
{
	if (HasAuthority() == false)
	{
		return;
	}
	
	RoundWinner = nullptr;
	bRoundDraw = true;
	
	UE_LOG(LogTemp, Warning, TEXT("[Server] < Round result > DRAW..!"));
	
	NotifyRoundResultChanged();
}

void AP48GameStateBase::ResetRoundResult()
{
	if (HasAuthority() == false)
	{
		return;
	}
	
	RoundWinner = nullptr;
	bRoundDraw = false;
	
	UE_LOG(LogTemp, Warning, TEXT("[Server] ~ Round result reset ~ "));
	
	NotifyRoundResultChanged();
}



bool AP48GameStateBase::HasRoundResult() const
{
	return IsValid(RoundWinner) || bRoundDraw;
}

void AP48GameStateBase::OnRep_RoundWinner()
{
	NotifyRoundResultChanged();
}

void AP48GameStateBase::OnRep_RoundDraw()
{
	NotifyRoundResultChanged();
}

/**
 * @brief UI담당자가 사용할 함수입니다.
 * 
 * bIsDraw == true : 무승부 UI 표시
 * Winner Is Valid : Winner의 PlayerName등을 사용해 승자 UI 표시
 * 둘 다 아님 : 결과 UI 숨기기
 */
void AP48GameStateBase::NotifyRoundResultChanged()
{
	UE_LOG(LogTemp,Log,
		TEXT("[%s] Round result changed: Winner=%s, Draw=%s"),
		HasAuthority() ? TEXT("Server") : TEXT("Client"),
		*GetNameSafe(RoundWinner),
		bRoundDraw ? TEXT("true") : TEXT("false"));

	OnRoundResultChanged.Broadcast(RoundWinner, bRoundDraw);
}
