// P48GameModeBase.cpp


#include "P48GameModeBase.h"
#include "TimerManager.h"

#include "P48GameStateBase.h"
#include "../Character/P48PlayerState.h"
#include "../Maps/PCG/Common/P48PCGNetworkSeedSettings.h"
#include "../Maps/PCG/Common/P48PCGSeedState.h"
#include "EngineUtils.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"


#include "GameFramework/PlayerController.h"

#include "Misc/Guid.h"

namespace P48MapReadiness
{
	bool RequiresNetworkSeed(const UWorld* World)
	{
		if (!World) { return false; }
		for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
		{
			TArray<UPCGComponent*> Components;
			ActorIt->GetComponents(Components);
			for (const UPCGComponent* Component : Components)
			{
				const UPCGGraph* Graph = Component ? Component->GetGraph() : nullptr;
				if (!Graph) { continue; }
				for (const UPCGNode* Node : Graph->GetNodes())
				{
					if (Node && Cast<UP48PCGNetworkSeedSettings>(Node->GetSettings())) { return true; }
				}
			}
		}
		return false;
	}

	AP48PCGSeedState* FindSeedState(const UWorld* World)
	{
		if (!World) { return nullptr; }
		TActorIterator<AP48PCGSeedState> It(World);
		if (It) { return *It; }
		return nullptr;
	}
}

void AP48GameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (!NewPlayer) { return; }
	if (AP48PCGSeedState* SeedState = P48MapReadiness::FindSeedState(GetWorld())) { SeedState->NotifyControllerJoined(NewPlayer); }
	const AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (bWaitingForRoundMap || (GS && GS->MatchPhase != EP48MatchPhase::Waiting
		&& !IsRoundSpawnParticipant(NewPlayer->GetPlayerState<AP48PlayerState>())))
	{
		return;
	}
	if (IsMapReadyForPlayer(NewPlayer))
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		return;
	}
	PlayersWaitingForMap.AddUnique(NewPlayer);
	UE_LOG(LogTemp, Display, TEXT("[P48MapReady] Delaying pawn spawn for %s"), *GetNameSafe(NewPlayer));
}

bool AP48GameModeBase::IsMapReadyForPlayer(const APlayerController* PlayerController) const
{
	if (!P48MapReadiness::RequiresNetworkSeed(GetWorld())) { return true; }
	const AP48PCGSeedState* SeedState = P48MapReadiness::FindSeedState(GetWorld());
	return SeedState && SeedState->IsMapReady() && SeedState->IsReadyForController(PlayerController);
}

void AP48GameModeBase::NotifyMapGenerationReadinessChanged()
{
	if (!HasAuthority()) { return; }
	if (bWaitingForRoundMap)
	{
		// 맵 준비 상태 변경 알림을 받은 뒤 다음 틱에서 라운드 준비 완료 여부를 확인한다.
		GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::TryFinishRoundMapPreparation);
		return;
	}
	const AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!GS || GS->MatchPhase == EP48MatchPhase::MatchEnd) { return; }
	SpawnPlayersWaitingForMap();
	CheckStartCondition();
}

void AP48GameModeBase::SpawnPlayersWaitingForMap()
{
	for (int32 Index = PlayersWaitingForMap.Num() - 1; Index >= 0; --Index)
	{
		APlayerController* PlayerController = PlayersWaitingForMap[Index].Get();
		if (!PlayerController)
		{
			PlayersWaitingForMap.RemoveAtSwap(Index);
			continue;
		}
		if (!IsMapReadyForPlayer(PlayerController)) { continue; }
		PlayersWaitingForMap.RemoveAtSwap(Index);
		Super::HandleStartingNewPlayer_Implementation(PlayerController);
		UE_LOG(LogTemp, Display, TEXT("[P48MapReady] Spawned pawn for %s"), *GetNameSafe(PlayerController));
	}
}


void AP48GameModeBase::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);
	
	UE_LOG(LogTemp, Warning, TEXT("Player Login: %s"), *GetNameSafe(NewPlayer));
	UE_LOG(LogTemp, Log, TEXT("Player Count: %d"), GetNumPlayers());
	
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false)
	{
		return;
	}

	AP48PlayerState* P48PlayerState = NewPlayer->GetPlayerState<AP48PlayerState>();
	if (IsValid(P48PlayerState) == false)
	{
		return;
	}

	// Waiting 이후 접속자는 현재 Match에 참가시키지 않는다. (중도 입장 불가!)
	if (P48GameState->MatchPhase != EP48MatchPhase::Waiting)
	{
		P48PlayerState->SetMatchParticipant(false);
		P48PlayerState->SetAlive(false);

		UE_LOG(LogTemp,Warning,TEXT("[Server] %s joined as a spectator for the current match"),*P48PlayerState->GetPlayerName());

		return;
	}
	
	CheckStartCondition();
}

void AP48GameModeBase::Logout(AController* Exit)
{
	PlayersWaitingForMap.Remove(Cast<APlayerController>(Exit));
    const FString ExitingPlayerName = GetNameSafe(Exit);
    const int32 RemainingPlayerCount = FMath::Max(0, GetNumPlayers() - 1);

    AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();

    AP48PlayerState* ExitingPlayerState = IsValid(Exit) ? Exit->GetPlayerState<AP48PlayerState>() : nullptr;

    const bool bWasMatchParticipant = IsValid(ExitingPlayerState) && ExitingPlayerState->IsMatchParticipant();

    int32 RemainingParticipantCount = 0;

    if (IsValid(P48GameState) == true)
    {
        for (APlayerState* PlayerState : P48GameState->PlayerArray)
        {
            AP48PlayerState* P48PlayerState = Cast<AP48PlayerState>(PlayerState);

            if (IsValid(P48PlayerState) == false)
            {
                continue;
            }

            // 지금 퇴장하는 플레이어는 남은 참가자 수에서 제외한다.
            if (P48PlayerState == ExitingPlayerState)
            {
                continue;
            }

            if (P48PlayerState->IsMatchParticipant() == true)
            {
                RemainingParticipantCount++;
            }
        }
    }

    Super::Logout(Exit);

    UE_LOG(LogTemp,Warning,TEXT("Player Logout: %s"),*ExitingPlayerName);

    UE_LOG(LogTemp,Warning,TEXT("Remaining Player Count: %d"),RemainingPlayerCount);

    UE_LOG(LogTemp,Warning,TEXT("Remaining Match Participants: %d"),RemainingParticipantCount);

    if (IsValid(P48GameState) == true
        && P48GameState->MatchPhase == EP48MatchPhase::Countdown
        && bWasMatchParticipant == true
        && ShouldCancelCountdown(RemainingParticipantCount))
    {
        GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
        P48GameState->SetMatchPhase(EP48MatchPhase::Waiting);

		for (APlayerState* PlayerState : P48GameState->PlayerArray)
		{
			AP48PlayerState* P48PlayerState = Cast<AP48PlayerState>(PlayerState);

			if (IsValid(P48PlayerState) == true)
			{
				P48PlayerState->SetMatchParticipant(false);
			}
		}

        UE_LOG(LogTemp,Warning,TEXT("[Server] Countdown canceled: not enough match participants"));
    }
}

void AP48GameModeBase::NotifyPlayerReadyStateChanged()
{
	UE_LOG(LogTemp,Warning,TEXT("[Server] Player Ready state changed"));

	CheckStartCondition();
}

void AP48GameModeBase::CheckStartCondition()
{
	if (!HasAuthority())
	{
		return;
	}

	if (P48MapReadiness::RequiresNetworkSeed(GetWorld()))
	{
		const AP48PCGSeedState* SeedState = P48MapReadiness::FindSeedState(GetWorld());
		if (!SeedState || !SeedState->IsMapReady()) { return; }
	}
	if (GetNumPlayers() < MinPlayersToStart)
	{
		return;
	}
	
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false)
	{
		return;
	}
	
	if (P48GameState->MatchPhase != EP48MatchPhase::Waiting)
	{
		return;
	}
	
	// 로비에서 레디 확인을 완료하므로 게임맵의 중복 레디 검사는 사용하지 않음
	// 예정 인원 전원 도착 여부는 로비 이동 흐름과 별도 연동 필요
	/*
	if (AreAllPlayersReady() == false)
	{
		UE_LOG(LogTemp,Warning,TEXT("[Server] Waiting for all players to be ready"));

		return;
	}
	*/
	
	P48GameState->ResetMatchResult();
	ConfirmMatchParticipants();

	UE_LOG(LogTemp,Warning,TEXT("[Server] Start condition met: %d/%d"),GetNumPlayers(),MinPlayersToStart);
	StartCountdown();
}

bool AP48GameModeBase::AreAllPlayersReady() const
{
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false)
	{
		return false;
	}
	
	int32 ValidPlayerCount = 0;
	
	for (APlayerState* PlayerState : P48GameState->PlayerArray)
	{
		const AP48PlayerState* P48PlayerState = Cast<AP48PlayerState>(PlayerState);
		if (IsValid(P48PlayerState) == false)
		{
			return false;
		}
		
		ValidPlayerCount++;
		
		if (P48PlayerState->IsReady() == false)
		{
			return false;
		}
	}
	
	return ValidPlayerCount >= MinPlayersToStart;
}

void AP48GameModeBase::ConfirmMatchParticipants()
{
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false)
	{
		return;
	}
	
	int32 ParticipantCount = 0;
	
	for (APlayerState* PlayerState : P48GameState->PlayerArray)
	{
		AP48PlayerState* P48PlayerState = Cast<AP48PlayerState>(PlayerState);
		if (IsValid(P48PlayerState) == false)
		{
			continue;
		}
		
		P48PlayerState->SetMatchParticipant(true);
		ParticipantCount++;
	}

	UE_LOG(LogTemp,Warning,TEXT("[Server] Match participants confirmed: %d"),ParticipantCount);
}

void AP48GameModeBase::StartCountdown()
{
	if (bWaitingForRoundMap) { return; }
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(P48GameState))
	{
		return;
	}

	if (P48GameState->MatchPhase != EP48MatchPhase::Waiting && P48GameState->MatchPhase != EP48MatchPhase::RoundEnd)
	{
		return;
	}
	
	P48GameState->SetMatchPhase(EP48MatchPhase::Countdown);
	
	UE_LOG(LogTemp,Warning,TEXT("[Server] Countdown started: %.1f sec"),CountdownDuration);
	
	GetWorldTimerManager().SetTimer(
		CountdownTimerHandle,
		this,
		&ThisClass::StartMatch,
		CountdownDuration,
		false);
}

void AP48GameModeBase::StartMatch()
{
	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !IsValid(P48GameState) || P48GameState->MatchPhase != EP48MatchPhase::Countdown)
	{
		return;
	}
	
	if (P48GameState->CurrentRound == 0)
	{
		P48GameState->SetCurrentRound(1);
	}
	
	P48GameState->SetMatchPhase(EP48MatchPhase::Playing);
	
	UE_LOG(LogTemp,Warning,TEXT("[Server] Round %d started"),P48GameState->CurrentRound);
	
}

void AP48GameModeBase::StartRoundEnd()
{
	if (!HasAuthority())
	{
		return;
	}

	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false)
	{
		return;
	}
	
	if (P48GameState->MatchPhase != EP48MatchPhase::Playing)
	{
		return;
	}
	
	P48GameState->SetMatchPhase(EP48MatchPhase::RoundEnd);
	
	UE_LOG(LogTemp,Warning,TEXT("[Server] Round %d ended"),P48GameState->CurrentRound);
	
	GetWorldTimerManager().SetTimer(
		RoundEndTimerHandle,
		this,
		&ThisClass::PrepareNextRound,
		RoundEndDuration,
		false);
}

void AP48GameModeBase::PrepareNextRound()
{
	if (!HasAuthority())
	{
		return;
	}

	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false)
	{
		return;
	}
	
	if (P48GameState->MatchPhase != EP48MatchPhase::RoundEnd || bWaitingForRoundMap)
	{
		return;
	}
	
	// 라운드 번호 증가 정책은 진행 규칙에 위임
	if (ShouldAdvanceRound())
	{
		P48GameState->SetCurrentRound(P48GameState->CurrentRound + 1);
	}
	
	UE_LOG(LogTemp,Warning,TEXT("[Server] Preparing Round %d"),P48GameState->CurrentRound);

	bWaitingForRoundMap = true;
	GetWorldTimerManager().ClearTimer(RoundEndTimerHandle);
	PlayersWaitingForMap.Reset();
	if (AP48PCGSeedState* SeedState = P48MapReadiness::FindSeedState(GetWorld()))
	{
		int32 NewSeed = static_cast<int32>(GetTypeHash(FGuid::NewGuid()) & MAX_int32);
		if (NewSeed == SeedState->State.Seed) { NewSeed = NewSeed == MAX_int32 ? 0 : NewSeed + 1; }
		SeedState->SetMapSeed(NewSeed);
	}
	// 완료 알림 외에도 퇴장으로 준비 조건이 풀리는 경우 재확인한다.
	GetWorldTimerManager().SetTimer(RoundMapPreparationTimerHandle, this, &ThisClass::TryFinishRoundMapPreparation, 0.1f, true);
}

bool AP48GameModeBase::IsRoundSpawnParticipant(const AP48PlayerState* Player) const
{
	return IsValid(Player) && Player->IsMatchParticipant();
}

void AP48GameModeBase::TryFinishRoundMapPreparation()
{
	AP48GameStateBase* GS = GetGameState<AP48GameStateBase>();
	if (!HasAuthority() || !bWaitingForRoundMap || !GS || GS->MatchPhase != EP48MatchPhase::RoundEnd)
	{
		bWaitingForRoundMap = false;
		GetWorldTimerManager().ClearTimer(RoundMapPreparationTimerHandle);
		return;
	}
	AP48PCGSeedState* SeedState = P48MapReadiness::FindSeedState(GetWorld());
	if (P48MapReadiness::RequiresNetworkSeed(GetWorld()) && (!SeedState || !SeedState->IsMapReady())) { return; }

	TArray<APlayerController*> Participants;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && IsRoundSpawnParticipant(PC->GetPlayerState<AP48PlayerState>()))
		{
			Participants.Add(PC);
		}
	}
	// 결정전 퇴장 결과는 기존 규칙이 확정한다.
	if (Participants.IsEmpty())
	{
		if (!GS->IsTiebreaker())
		{
			ClearMatchTimers();
			GS->SetCurrentRound(0);
			GS->ResetRoundResult();
			GS->ResetMatchResult();
			GS->SetMatchPhase(EP48MatchPhase::Waiting);
		}
		return;
	}

	// TODO: 플레이어 담당의 라운드 복구/재배치 완료 처리와 연결한다.
	bWaitingForRoundMap = false;
	GetWorldTimerManager().ClearTimer(RoundMapPreparationTimerHandle);
	UE_LOG(LogTemp, Display, TEXT("[RoundMap] Ready!! Participants=%d"), Participants.Num());
	// TODO: 플레이어 담당의 Pawn 재생성·재배치 완료 처리와 연결한 뒤 카운트다운을 시작하도록 변경한다.
	StartCountdown();
}

bool AP48GameModeBase::ShouldCancelCountdown(int32 RemainingParticipants) const
{
	return RemainingParticipants < MinPlayersToStart;
}

void AP48GameModeBase::ClearMatchTimers()
{
	bWaitingForRoundMap = false;
	GetWorldTimerManager().ClearTimer(RoundMapPreparationTimerHandle);
	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	GetWorldTimerManager().ClearTimer(RoundEndTimerHandle);
}

void AP48GameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearMatchTimers();
	Super::EndPlay(EndPlayReason);
}
