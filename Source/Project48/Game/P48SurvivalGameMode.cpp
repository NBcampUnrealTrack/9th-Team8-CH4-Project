// P48SurvivalGameMode.cpp

#include "P48SurvivalGameMode.h"

#include "P48GameStateBase.h"
#include "P48GameServerLifecycleSubsystem.h"
#include "Engine/GameInstance.h"
#include "Project48/Character/P48PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpectatorPawn.h"
#include "UObject/ConstructorHelpers.h"
#include "Rule/P48BestOfRoundsMatchFlowRule.h"
#include "Rule/P48LastPlayerStandingCondition.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Project48/Character/P48PlayerController.h"

AP48SurvivalGameMode::AP48SurvivalGameMode()
{
	static ConstructorHelpers::FClassFinder<ASpectatorPawn> SpectatorBP(
		TEXT("/Game/OJH/Spectator/BP_P48SpectatorPawn"));
	if (SpectatorBP.Succeeded()) { SpectatorClass = SpectatorBP.Class; }
	MatchFlowRuleClass = UP48BestOfRoundsMatchFlowRule::StaticClass();
	RoundWinConditionClass = UP48LastPlayerStandingCondition::StaticClass();
}

FString AP48SurvivalGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString SuperError = Super::InitNewPlayer(
		NewPlayerController, UniqueId, Options, Portal);
	if (!SuperError.IsEmpty()) return SuperError;
    
	UE_LOG(LogTemp, Log, TEXT("INP 들어옴"));
    
	const FString Nickname = UGameplayStatics::ParseOption(Options, TEXT("Nickname")).TrimStartAndEnd();
	if (Nickname.IsEmpty())
	{
		return TEXT("Error2");
	}
    
	AP48PlayerController* P48PC = Cast<AP48PlayerController>(NewPlayerController);
	if (IsValid(P48PC) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("INP P48PC 생성 실패"));
		return TEXT("Error3");
	}
	AP48PlayerState* P48PS = P48PC->GetPlayerState<AP48PlayerState>();
	if (IsValid(P48PS) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("INP P48PS 생성 실패"));
		return TEXT("Error4");
	}
	UE_LOG(LogTemp, Log, TEXT("INP Nick[%s]"), *Nickname);
	P48PS->SetNickname(Nickname);
	UE_LOG(LogTemp, Log, TEXT("INP PSNick[%s]"), *P48PS->GetNickname());
	return TEXT("");
}

void AP48SurvivalGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (!HasAuthority())
	{
		return;
	}

	// 플레이어 접속과 카운트다운 전에 규칙 객체 생성
	UClass* FlowClass = MatchFlowRuleClass.Get();
	UClass* ConditionClass = RoundWinConditionClass.Get();
	if (!FlowClass || FlowClass->HasAnyClassFlags(CLASS_Abstract))
	{
		FlowClass = UP48BestOfRoundsMatchFlowRule::StaticClass();
	}
	if (!ConditionClass || ConditionClass->HasAnyClassFlags(CLASS_Abstract))
	{
		ConditionClass = UP48LastPlayerStandingCondition::StaticClass();
	}

	MatchFlowRule = NewObject<UP48MatchFlowRule>(this, FlowClass);
	RoundWinCondition = NewObject<UP48RoundWinCondition>(this, ConditionClass);
	MatchFlowRule->Initialize(DefaultRoundCount);
}

bool AP48SurvivalGameMode::IsCurrentRoundParticipant(const AP48PlayerState* Player) const
{
	const AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	return IsValid(GS) && IsValid(MatchFlowRule) && MatchFlowRule->IsRoundParticipant(*GS, Player);
}

bool AP48SurvivalGameMode::IsRoundSpawnParticipant(const AP48PlayerState* Player) const
{
	return IsCurrentRoundParticipant(Player);
}

bool AP48SurvivalGameMode::IsTiebreakerParticipant(const AP48PlayerState* Player) const
{
	return IsValid(MatchFlowRule) && MatchFlowRule->IsTiebreakerParticipant(Player);
}

FP48RoundResult AP48SurvivalGameMode::EvaluateRound() const
{
	const AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!IsValid(GS) || !IsValid(MatchFlowRule) || !IsValid(RoundWinCondition))
	{
		return {};
	}

	TArray<AP48PlayerState*> Participants;
	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		AP48PlayerState* Player = Cast<AP48PlayerState>(PlayerState);
		if (MatchFlowRule->IsRoundParticipant(*GS, Player))
		{
			Participants.Add(Player);
		}
	}

	return RoundWinCondition->Evaluate(Participants, GS->IsRoundTimeExpired());
}

void AP48SurvivalGameMode::NotifyPlayerEliminated(AP48PlayerState* EliminatedPlayer)
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS) || GS->MatchPhase != EP48MatchPhase::Playing
		|| !IsCurrentRoundParticipant(EliminatedPlayer) || !EliminatedPlayer->IsAlive())
	{
		return;
	}

	EliminatedPlayer->SetAlive(false);
	if (APlayerController* PC = Cast<APlayerController>(EliminatedPlayer->GetOwner()))
	{
		SetPlayerInputBlocked(PC, true);
		const TWeakObjectPtr<APawn> DeadPawn = PC->GetPawn();
		FTimerHandle DeathTimer;
		GetWorldTimerManager().SetTimer(DeathTimer, FTimerDelegate::CreateWeakLambda(this, [this, DeadPawn]()
		{
			// 새 Pawn이 아닌 사망 당시 Pawn만 제거한다.
			if (APawn* Pawn = DeadPawn.Get())
			{
				APlayerController* Controller = Cast<APlayerController>(Pawn->GetController());
				if (Controller) { Controller->UnPossess(); }
				Pawn->Destroy();
				if (Controller) { StartPlayerSpectating(Controller); }
			}
		}), 3.0f, false);
	}
	UE_LOG(LogTemp, Warning, TEXT("[Server] Player eliminated: %s, Alive participants: %d"),
		*EliminatedPlayer->GetPlayerName(), EvaluateRound().AliveCount);

	RoundEndCheck();
}

void AP48SurvivalGameMode::RoundEndCheck()
{
	if (bRoundEndCheck)
	{
		return;
	}

	bRoundEndCheck = true;
	GetWorldTimerManager().SetTimer(
		RoundEndCheckTimerHandle, this, &ThisClass::CheckRoundEndCondition, RoundEndCheckDelay, false);
}

void AP48SurvivalGameMode::CheckRoundEndCondition()
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS) || !IsValid(MatchFlowRule)
		|| GS->MatchPhase != EP48MatchPhase::Playing || GS->HasRoundResult())
	{
		bRoundEndCheck = false;
		return;
	}

	// 연속 퇴장을 모으는 동안 기존 사망 판정이 먼저 승자를 확정하지 않도록 한다.
	if (GetWorldTimerManager().IsTimerActive(LogoutCheckTimerHandle))
	{
		bRoundEndCheck = false;
		return;
	}

	const FP48RoundResult Round = EvaluateRound();
	UE_LOG(LogTemp, Warning, TEXT("[Server] Round end check: Alive participants = %d"), Round.AliveCount);
	if (Round.Outcome == EP48RoundOutcome::InProgress)
	{
		bRoundEndCheck = false;
		return;
	}

	const FP48MatchFlowResult Flow = MatchFlowRule->ResolveRound(*GS, Round);
	if (Flow.Action == EP48MatchFlowAction::None)
	{
		bRoundEndCheck = false;
		return;
	}

	// 결과 확정 전에 예약된 검사를 취소하여 사망과 시간 만료의 중복 반영 방지
	ClearMatchTimers();

	// 복제 상태 변경과 승수 반영은 서버 GameMode에서 처리
	if (Round.Outcome == EP48RoundOutcome::Winner)
	{
		// 승수 먼저 증가
		if (Flow.bAwardRoundWin)
		{
			Round.Winner->AddRoundWin();
		}
		
		// 승수가 증가한 이후 라운드 승자를 설정한다.
		GS->SetRoundWinner(Round.Winner);
	}
	else
	{
		GS->SetRoundDraw();
	}

	//  Match가 실제로 끝나는 경우, 승수 반영이 끝난 상태에서 최종 랭킹을 확정
	if (Flow.Action == EP48MatchFlowAction::MatchWinner || Flow.Action == EP48MatchFlowAction::MatchDraw)
	{
		GS->BuildFinalRankingData();
	}
	
	ApplyFlowResult(Flow);
}

void AP48SurvivalGameMode::ResetParticipantsForRound()
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!IsValid(GS))
	{
		return;
	}

	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		AP48PlayerState* Player = Cast<AP48PlayerState>(PlayerState);
		if (IsValid(Player))
		{
			Player->ResetForNewRound();
			if (!IsCurrentRoundParticipant(Player))
			{
				Player->SetAlive(false);
			}
		}
	}
}

void AP48SurvivalGameMode::StartMatch()
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS) || !IsValid(MatchFlowRule) || !IsValid(RoundWinCondition)
		|| GS->MatchPhase != EP48MatchPhase::Countdown)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RoundEndCheckTimerHandle);
	bRoundEndCheck = false;
	GS->ResetRoundResult();
	ResetParticipantsForRound();
	Super::StartMatch();

	// 일반 라운드와 결정전 모두 Playing 진입 시 같은 제한 시간 적용
	const float Duration = FMath::Max(1.0f, RoundTimeLimit);
	GS->SetRoundEndServerTime(GS->GetServerWorldTimeSeconds() + Duration);
	GetWorldTimerManager().SetTimer(
		RoundTimeLimitTimerHandle, this, &ThisClass::HandleRoundTimeExpired, Duration, false);

	UE_LOG(LogTemp, Warning, TEXT("[Server] Survival round started: Alive participants = %d"), EvaluateRound().AliveCount);
}

void AP48SurvivalGameMode::HandleRoundTimeExpired()
{
	// 기존 지연 검사 경로에서 생존자와 시간 만료 여부를 규칙에 함께 전달
	RoundEndCheck();
}

void AP48SurvivalGameMode::Logout(AController* Exit)
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	AP48PlayerState* Player = IsValid(Exit) ? Exit->GetPlayerState<AP48PlayerState>() : nullptr;

	// 사망 여부와 관계없이 접속 중인 매치 참가자의 퇴장을 검사한다.
	const bool bParticipantLeft = IsValid(Player) && Player->IsMatchParticipant()
		&& IsValid(GS) && GS->MatchPhase != EP48MatchPhase::MatchEnd
		&& (GS->MatchPhase != EP48MatchPhase::Waiting || bMapGenerationRequested);
	// Super::Logout 호출 전에 퇴장자를 규칙의 참가자 목록에서 제거
	const bool bCheckLogout = HasAuthority() && IsValid(GS) && IsValid(MatchFlowRule)
		&& MatchFlowRule->RemoveParticipant(*GS, Player);
	if (bCheckLogout)
	{
		Player->SetAlive(false);
	}
	else if (IsValid(GS) && GS->MatchPhase == EP48MatchPhase::Playing)
	{
		NotifyPlayerEliminated(Player);
	}

	Super::Logout(Exit);
	if (IsValid(Player)) Player->SetMatchParticipant(false);

	if (bCheckLogout || bParticipantLeft)
	{
		GetWorldTimerManager().SetTimer(
			LogoutCheckTimerHandle, this, &ThisClass::CheckAfterLogout,
			FMath::Max(0.01f, RoundEndCheckDelay), false);
	}
}

void AP48SurvivalGameMode::CheckAfterLogout()
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS) || !IsValid(MatchFlowRule))
	{
		return;
	}

	if (GS->MatchPhase == EP48MatchPhase::MatchEnd) return;
	int32 Remaining = 0;
	AP48PlayerState* LastParticipant = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AP48PlayerState* Player = It->Get() ? It->Get()->GetPlayerState<AP48PlayerState>() : nullptr;
		if (IsValid(Player) && Player->IsMatchParticipant())
		{
			++Remaining;
			LastParticipant = Player;
		}
	}
	if (Remaining <= 1)
	{
		FinishMatch(LastParticipant);
		return;
	}
	ApplyFlowResult(MatchFlowRule->ResolveLogout(*GS));
	// 퇴장 검사 때문에 보류된 일반 라운드 생존자 판정을 다시 실행한다.
	if (!GS->IsTiebreaker() && GS->MatchPhase == EP48MatchPhase::Playing) RoundEndCheck();
}

void AP48SurvivalGameMode::ApplyFlowResult(const FP48MatchFlowResult& Result)
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS))
	{
		return;
	}

	switch (Result.Action)
	{
	case EP48MatchFlowAction::StartTiebreaker:
		GS->SetTiebreaker(true);
		UE_LOG(LogTemp, Log, TEXT("[Server] Tiebreaker scheduled"));
		StartRoundEnd();
		break;
	case EP48MatchFlowAction::NextRound:
		StartRoundEnd();
		break;
	case EP48MatchFlowAction::MatchWinner:
		if (IsValid(Result.Winner))
		{
			FinishMatch(Result.Winner);
		}
		break;
	case EP48MatchFlowAction::MatchDraw:
		FinishMatch(nullptr);
		break;
	case EP48MatchFlowAction::CheckRound:
		RoundEndCheck();
		break;
	case EP48MatchFlowAction::None:
		break;
	}
}

void AP48SurvivalGameMode::FinishMatch(AP48PlayerState* Winner)
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(GS)
		|| (GS->MatchPhase == EP48MatchPhase::Waiting && !bMapGenerationRequested)
		|| GS->MatchPhase == EP48MatchPhase::MatchEnd)
	{
		return;
	}

	// 승리와 무승부 모두 같은 경로에서 타이머 및 규칙 정보 정리
	ClearMatchTimers();
	if (IsValid(Winner))
	{
		GS->SetMatchWinner(Winner);
	}
	else
	{
		GS->ResetMatchResult();
		UE_LOG(LogTemp, Log, TEXT("[Server] Match ended in a draw"));
	}

	GS->SetTiebreaker(false);
	if (IsValid(MatchFlowRule))
	{
		MatchFlowRule->Reset();
	}
	GS->SetMatchPhase(EP48MatchPhase::MatchEnd);
	GetGameInstance()->GetSubsystem<UP48GameServerLifecycleSubsystem>()->EndMatch();
	SetRoundInputBlocked(true);
	GS->ForceNetUpdate();
	GetWorldTimerManager().SetTimer(
		ReturnLobbyTimerHandle, this, &ThisClass::RequestLobbyReturn,
		FMath::Max(0.1f, ReturnLobbyDelay), false);
}

void AP48SurvivalGameMode::RequestLobbyReturn()
{
	if (!HasAuthority()) return;
	if (AP48GameStateBase* GS = GetGameState<AP48GameStateBase>())
	{
		GS->RequestLobbyReturn();
	}
	GetWorldTimerManager().SetTimer(ServerResetTimerHandle, this, &ThisClass::TryResetServer, 1.0f, true);
}

void AP48SurvivalGameMode::TryResetServer()
{
	// 복귀 요청만 보낸 상태에서 초기화하지 않고, 관전자를 포함한 접속 종료를 기다린다.
	if (GetWorld()->GetNumPlayerControllers() != 0) return;
	GetWorldTimerManager().ClearTimer(ServerResetTimerHandle);
	GetGameInstance()->GetSubsystem<UP48GameServerLifecycleSubsystem>()->BeginReload();
	bUseSeamlessTravel = false;
	const FString Map = GetWorld()->GetOutermost()->GetName();
	// Absolute travel로 이전 매치의 URL 옵션을 재사용하지 않는다.
	if (!GetWorld()->ServerTravel(Map, true))
	{
		UE_LOG(LogTemp, Error, TEXT("[GameServer] Could not reload %s; server remains unavailable."), *Map);
	}
}

void AP48SurvivalGameMode::ClearMatchTimers()
{
	Super::ClearMatchTimers();
	GetWorldTimerManager().ClearTimer(ReturnLobbyTimerHandle);
	GetWorldTimerManager().ClearTimer(ServerResetTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundEndCheckTimerHandle);
	GetWorldTimerManager().ClearTimer(LogoutCheckTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundTimeLimitTimerHandle);
	if (AP48GameStateBase* GS = GetGameState<AP48GameStateBase>())
	{
		GS->SetRoundEndServerTime(0.0);
	}
	bRoundEndCheck = false;
}

bool AP48SurvivalGameMode::ShouldAdvanceRound() const
{
	const AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	return IsValid(GS) && IsValid(MatchFlowRule) && MatchFlowRule->ShouldAdvanceRound(*GS);
}

bool AP48SurvivalGameMode::ShouldCancelCountdown(int32 RemainingParticipants) const
{
	const AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	return IsValid(GS) && IsValid(MatchFlowRule)
		&& MatchFlowRule->ShouldCancelCountdown(*GS, RemainingParticipants, MinPlayersToStart);
}
