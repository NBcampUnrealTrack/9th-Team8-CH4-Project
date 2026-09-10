// P48GameModeBase.cpp


#include "P48GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "../GameplayMessageLibrary/Core/P48GameplayMessageLibrary.h"
#include "../GameplayMessageLibrary/Core/P48GameplayMessageTags.h"
#include "../GameplayMessageLibrary/Map/P48MapMessagePayloads.h"
#include "../GameplayMessageLibrary/Match/P48MatchMessagePayloads.h"
#include "TimerManager.h"

#include "P48GameStateBase.h"
#include "../Character/P48PlayerState.h"
#include "../Maps/PCG/Common/P48PCGSeedWorldSubsystem.h"
#include "../Maps/PCG/Common/P48PCGSeedState.h"
#include "EngineUtils.h"


#include "GameFramework/PlayerController.h"


namespace P48MapReadiness
{
	bool RequiresNetworkSeed(UWorld* World)
	{
		return UP48PCGSeedWorldSubsystem::HasNetworkSeedConsumer(World);
	}

	AP48PCGSeedState* FindSeedState(const UWorld* World)
	{
		if (!World) { return nullptr; }
		TActorIterator<AP48PCGSeedState> It(World);
		if (It) { return *It; }
		return nullptr;
	}
}

void AP48GameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	ConfirmedPlayerCount = UGameplayStatics::GetIntOption(Options, TEXT("ExpectedPlayers"), 0);
	if (ConfirmedPlayerCount <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[Server] Missing or invalid ExpectedPlayers: %d"), ConfirmedPlayerCount);
	}
}

void AP48GameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (!NewPlayer) { return; }
	const AP48PlayerState* Player = NewPlayer->GetPlayerState<AP48PlayerState>();
	if (bMapGenerationRequested && (!Player || !Player->IsMatchParticipant())) { return; }
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
		const AP48PlayerState* Player = PlayerController->GetPlayerState<AP48PlayerState>();
		if (bMapGenerationRequested && (!Player || !Player->IsMatchParticipant()))
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

	// 맵 생성 요청 이후 접속자는 현재 매치에 참가시키지 않는다.
	if (bMapGenerationRequested || P48GameState->MatchPhase != EP48MatchPhase::Waiting)
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

	if (bMapGenerationRequested && bWasMatchParticipant)
	{
		ConfirmedPlayerCount = RemainingParticipantCount;
		UP48GameplayMessageLibrary::Broadcast(
			this,
			P48GameplayTags::Match::PlayerCountChanged,
			FP48MatchPlayerCountMessage(RemainingParticipantCount));
	}

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

			// 생성 요청 시 확정한 명단은 카운트다운이 취소되어도 유지한다.
			if (IsValid(P48PlayerState) == true && !bMapGenerationRequested)
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

	AP48GameStateBase* P48GameState = GetGameState<AP48GameStateBase>();
	if (IsValid(P48GameState) == false)
	{
		return;
	}
	
	if (P48GameState->MatchPhase != EP48MatchPhase::Waiting)
	{
		return;
	}
	
	if (ConfirmedPlayerCount <= 0)
	{
		return;
	}

	int32 ArrivedPlayerCount = GetNumPlayers();
	if (bMapGenerationRequested)
	{
		ArrivedPlayerCount = 0;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const AP48PlayerState* Player = It->Get() ? It->Get()->GetPlayerState<AP48PlayerState>() : nullptr;
			if (IsValid(Player) && Player->IsMatchParticipant()) { ++ArrivedPlayerCount; }
		}
	}
	if (ArrivedPlayerCount != ConfirmedPlayerCount || ArrivedPlayerCount < MinPlayersToStart)
	{
		return;
	}

	if (!bMapGenerationRequested)
	{
		ConfirmMatchParticipants();

		// 참가 명단을 먼저 잠근다. PCG 맵인 경우에만 Maps에 실제 생성을 요청한다.
		bMapGenerationRequested = true;
		if (P48MapReadiness::RequiresNetworkSeed(GetWorld()))
		{
			const FP48MapGenerationRequestMessage Message(ConfirmedPlayerCount, 0);
			if (!UP48GameplayMessageLibrary::Broadcast(this, P48GameplayTags::Map::GenerationRequested, Message))
			{
				bMapGenerationRequested = false;
				UE_LOG(LogTemp, Error, TEXT("[Server] Map generation Broadcast failed."));
				return;
			}
		}
		if (P48GameState->MatchPhase != EP48MatchPhase::Waiting) { return; }
	}

	if (P48MapReadiness::RequiresNetworkSeed(GetWorld()))
	{
		const AP48PCGSeedState* SeedState = P48MapReadiness::FindSeedState(GetWorld());
		if (!SeedState || !SeedState->IsMapReady()) { return; }
		// 로그인 직후에는 전체 준비 상태에 새 Controller가 아직 반영되지 않을 수 있다.
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const AP48PlayerState* Player = It->Get() ? It->Get()->GetPlayerState<AP48PlayerState>() : nullptr;
			if (!IsValid(Player) || !Player->IsMatchParticipant()) { continue; }
			if (!SeedState->IsReadyForController(It->Get())) { return; }
		}
	}
	
	P48GameState->ResetMatchResult();

	UE_LOG(LogTemp,Warning,TEXT("[Server] Start condition met: %d/%d"),ArrivedPlayerCount,ConfirmedPlayerCount);
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

	int32 ParticipantCount = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (PC && IsRoundSpawnParticipant(PC->GetPlayerState<AP48PlayerState>()))
		{
			++ParticipantCount;
		}
	}
	ConfirmedPlayerCount = ParticipantCount;

	if (P48MapReadiness::RequiresNetworkSeed(GetWorld()))
	{
		const FP48MapGenerationRequestMessage Message(ParticipantCount, 0);
		if (ParticipantCount <= 0 || !UP48GameplayMessageLibrary::Broadcast(this, P48GameplayTags::Map::GenerationRequested, Message))
		{
			bWaitingForRoundMap = false;
			UE_LOG(LogTemp, Error, TEXT("[RoundMap] Failed to request map generation. Participants=%d"), ParticipantCount);
			return;
		}
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
