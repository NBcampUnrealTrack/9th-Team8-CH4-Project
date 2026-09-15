// P48GameStateBase.cpp


#include "P48GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Project48/Character/P48PlayerState.h"
#include "Engine/GameInstance.h"
#include "Project48/Character/P48PlayerController.h"
#include "Project48/Lobby/P48LobbyTravelSubsystem.h"
#include "Project48/UI/P48UIManagerComponent.h"

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
	DOREPLIFETIME(AP48GameStateBase, bLobbyReturnRequested);
	DOREPLIFETIME(AP48GameStateBase, CurrentRound);
	DOREPLIFETIME(AP48GameStateBase, RoundWinner);
	DOREPLIFETIME(AP48GameStateBase, bRoundDraw);
	DOREPLIFETIME(AP48GameStateBase, MatchWinner);
	DOREPLIFETIME(AP48GameStateBase, bIsTiebreaker);
	DOREPLIFETIME(AP48GameStateBase, RoundEndServerTime);
}

void AP48GameStateBase::RequestLobbyReturn()
{
	if (!HasAuthority() || MatchPhase != EP48MatchPhase::MatchEnd || bLobbyReturnRequested)
	{
		return;
	}

	bLobbyReturnRequested = true;
	ForceNetUpdate();
}

void AP48GameStateBase::OnRep_LobbyReturnRequested()
{
	if (!bLobbyReturnRequested || GetNetMode() != NM_Client) return;
	
	UGameInstance* GI = GetGameInstance();
	UP48LobbyTravelSubsystem* TravelSubsystem = GI ? GI->GetSubsystem<UP48LobbyTravelSubsystem>() : nullptr;
	if (!IsValid(TravelSubsystem) || !TravelSubsystem->HasLobbyReturnContext())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Match] Cannot return to lobby"));
		return;
	}
	if (!TravelSubsystem->ReturnToLobby())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Match] Lobby return could not be started / already in progress."));
	}
}

void AP48GameStateBase::SetRoundEndServerTime(double NewEndTime)
{
	if (!HasAuthority())
	{
		return;
	}

	RoundEndServerTime = NewEndTime;
	ForceNetUpdate();
}

float AP48GameStateBase::GetRemainingRoundTime() const
{
	if (MatchPhase != EP48MatchPhase::Playing || RoundEndServerTime <= 0.0)
	{
		return 0.0f;
	}

	return static_cast<float>(FMath::Max(0.0, RoundEndServerTime - GetServerWorldTimeSeconds()));
}

bool AP48GameStateBase::IsRoundTimeExpired() const
{
	return MatchPhase == EP48MatchPhase::Playing && RoundEndServerTime > 0.0
		&& GetServerWorldTimeSeconds() >= RoundEndServerTime;
}

void AP48GameStateBase::SetMatchPhase(EP48MatchPhase NewMatchPhase)
{
	if (HasAuthority() == false)
	{
		return;
	}
	
	// 서버 Phase전환 확인용 로그입니당.
	UE_LOG(LogTemp, Warning, TEXT("[Server] MatchPhase: %s -> %s"), 
		*UEnum::GetValueAsString(MatchPhase),
		*UEnum::GetValueAsString(NewMatchPhase));
	
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
	OnRep_CurrentRound();
	
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

void AP48GameStateBase::SetMatchWinner(AP48PlayerState* NewMatchWinner)
{
	if (!HasAuthority() || !IsValid(NewMatchWinner) || MatchWinner == NewMatchWinner)
	{
		return;
	}

	MatchWinner = NewMatchWinner;

	UE_LOG(LogTemp, Warning,
		TEXT("[Server] Match winner: %s, Wins: %d"),
		*NewMatchWinner->GetPlayerName(),
		NewMatchWinner->GetRoundWinCount());
}

void AP48GameStateBase::ResetMatchResult()
{
	if (!HasAuthority())
	{
		return;
	}

	MatchWinner = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[Server] Match result reset"));
}

void AP48GameStateBase::OnRep_CurrentRound()
{
	OnCurrentRoundChanged.Broadcast(CurrentRound);
}

void AP48GameStateBase::OnRep_MatchWinner()
{
	UE_LOG(LogTemp, Log, TEXT("[Client] Match winner replicated: %s"), *GetNameSafe(MatchWinner.Get()));
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
 * @brief 현재 라운드 결과를 로그로 출력하고 UI용 델리게이트를 호출합니다.
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

void AP48GameStateBase::SetTiebreaker(bool bNewTiebreaker)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsTiebreaker == bNewTiebreaker)
	{
		return;
	}

	bIsTiebreaker = bNewTiebreaker;

	UE_LOG(LogTemp, Log, TEXT("[Server] Tiebreaker state: %s"), bIsTiebreaker ? TEXT("true") : TEXT("false"));
}

/* UI */
void AP48GameStateBase::MulticastReceiveChatMessage_Implementation(const FChatMessage& InChatMessage)
{
	/* 추후에 PC변경 */
	AP48PlayerController* PC = Cast<AP48PlayerController>(GetWorld()->GetFirstPlayerController());
	if (IsValid(PC) == false) return;
	UP48UIManagerComponent* UIManager = PC->FindComponentByClass<UP48UIManagerComponent>();
	if (IsValid(UIManager) == false) return;
	
	UIManager->PrintChatMessageString(InChatMessage);
}