// P48GameModeBase.cpp


#include "P48GameModeBase.h"
#include "P48GameServerLifecycleSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "../GameplayMessageLibrary/Core/P48GameplayMessageLibrary.h"
#include "../GameplayMessageLibrary/Core/P48GameplayMessageTags.h"
#include "../GameplayMessageLibrary/Match/P48MatchMessagePayloads.h"
#include "TimerManager.h"

#include "P48GameStateBase.h"
#include "../Character/P48PlayerState.h"
#include "../Character/P48PlayerController.h"
#include "../Character/P48PlayerCharacter.h"
#include "../Maps/Objects/Spawn/P48PlayerStart.h"
#include "../Maps/Objects/Spawn/P48PlayerStartRegistrySubsystem.h"
#include "../Maps/PCG/Common/P48PCGSeedState.h"
#include "../Maps/PCG/Common/P48PCGSeedWorldSubsystem.h"
#include "EngineUtils.h"


#include "GameFramework/Pawn.h"
#include "GameFramework/SpectatorPawn.h"
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
	if (UGameplayStatics::HasOption(Options, TEXT("ExpectedPlayers")) && ConfirmedPlayerCount <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[Server] Missing or invalid ExpectedPlayers: %d"), ConfirmedPlayerCount);
	}
}

void AP48GameModeBase::StartPlay()
{
	Super::StartPlay();
	GetGameInstance()->GetSubsystem<UP48GameServerLifecycleSubsystem>()->WorldReady();
}

FString AP48GameModeBase::InitNewPlayer(APlayerController* NewPlayerController,
	const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	UP48GameServerLifecycleSubsystem* Lifecycle = GetGameInstance()->GetSubsystem<UP48GameServerLifecycleSubsystem>();
	if (!bMapGenerationRequested && UGameplayStatics::HasOption(Options, TEXT("LobbyMatchId"))
		&& !UGameplayStatics::HasOption(Options, TEXT("ExpectedPlayers")))
	{
		return TEXT("Spectators can join after the initial participants arrive.");
	}
	const FString Error = Lifecycle->AcceptMatch(Options, false);
	if (!Error.IsEmpty()) return Error;
	const FString SuperError = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!SuperError.IsEmpty()) return SuperError;
	Lifecycle->AcceptMatch(Options);
	if (!bMapGenerationRequested && Lifecycle->GetExpectedPlayers() > 0)
	{
		ConfirmedPlayerCount = Lifecycle->GetExpectedPlayers();
	}
	return FString();
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

AActor* AP48GameModeBase::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	if (!Player || !P48MapReadiness::RequiresNetworkSeed(GetWorld()))
	{
		return Super::FindPlayerStart_Implementation(Player, IncomingName);
	}

	const AP48PCGSeedState* SeedState = P48MapReadiness::FindSeedState(GetWorld());
	UP48PlayerStartRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UP48PlayerStartRegistrySubsystem>();
	TArray<AP48PlayerStart*> PlayerStarts;
	if (!SeedState || !SeedState->IsMapReady() || !Registry
		|| !Registry->GetReadyPlayerStarts(SeedState->State.Revision, PlayerStarts))
	{
		return Super::FindPlayerStart_Implementation(Player, IncomingName);
	}

	TArray<APlayerController*> Participants;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (PlayerController && IsRoundSpawnParticipant(PlayerController->GetPlayerState<AP48PlayerState>()))
		{
			Participants.Add(PlayerController);
		}
	}

	const int32 ParticipantIndex = Participants.IndexOfByKey(Cast<APlayerController>(Player));
	if (PlayerStarts.IsValidIndex(ParticipantIndex))
	{
		AP48PlayerStart* SelectedStart = PlayerStarts[ParticipantIndex];
		UE_LOG(LogTemp, Display,
			TEXT("[InitialSpawn] Selected PCG PlayerStart for %s. Slot=%d Location=%s Generation=%d"),
			*GetNameSafe(Player), SelectedStart->SpawnSlotIndex,
			*SelectedStart->GetActorLocation().ToCompactString(), SeedState->State.Revision);
		return SelectedStart;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[InitialSpawn] No PCG PlayerStart assignment for %s. Participants=%d Starts=%d Generation=%d"),
		*GetNameSafe(Player), Participants.Num(), PlayerStarts.Num(), SeedState->State.Revision);
	return Super::FindPlayerStart_Implementation(Player, IncomingName);
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
	InputBlockedControllers.Remove(Cast<APlayerController>(Exit));
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

	// 잠긴 참가자가 나간 경우에만 PlayerStart 요구 수를 줄인다.
	// 늦게 접속한 관전자는 참가자 수와 PlayerStart 레이아웃을 늘리지 않는다.
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
		const UP48PCGSeedWorldSubsystem* Coordinator = GetWorld()->GetSubsystem<UP48PCGSeedWorldSubsystem>();
		if (P48MapReadiness::RequiresNetworkSeed(GetWorld()) && (!Coordinator || !Coordinator->CanReceivePlayerCount())) { return; }
		ConfirmMatchParticipants();

		// 최초 확정 인원만 PlayerStart 정책에 전달한다. 맵 Seed와 생성 시작은 Maps가 관리한다.
		bMapGenerationRequested = true;
		if (!UP48GameplayMessageLibrary::Broadcast(
			this,
			P48GameplayTags::Match::PlayerCountChanged,
			FP48MatchPlayerCountMessage(ConfirmedPlayerCount)))
		{
			bMapGenerationRequested = false;
			UE_LOG(LogTemp, Error, TEXT("[Server] Confirmed player-count Broadcast failed."));
			return;
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
	
	SetRoundInputBlocked(true);
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
	SetRoundInputBlocked(false);
	
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
	
	SetRoundInputBlocked(true);
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
	// 생존자(결정전 비참가자 포함)는 지형 제거 전에 정리한다.
	// 탈락 Pawn은 사망 시 예약한 3초 삭제를 유지한다.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		const AP48PlayerState* Player = PC ? PC->GetPlayerState<AP48PlayerState>() : nullptr;
		if (Player && Player->IsAlive() && !PC->GetPawn<ASpectatorPawn>())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				PC->UnPossess();
				Pawn->Destroy();
			}
			if (!IsRoundSpawnParticipant(Player)) { StartPlayerSpectating(PC); }
		}
	}

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
		UP48PCGSeedWorldSubsystem* Coordinator = GetWorld()->GetSubsystem<UP48PCGSeedWorldSubsystem>();
		if (ParticipantCount <= 0
			|| !Coordinator
			|| !Coordinator->CanReceivePlayerCount()
			|| !UP48GameplayMessageLibrary::Broadcast(
				this,
				P48GameplayTags::Match::PlayerCountChanged,
				FP48MatchPlayerCountMessage(ParticipantCount)))
		{
			bWaitingForRoundMap = false;
			UE_LOG(LogTemp, Error, TEXT("[RoundMap] Failed to deliver player count. Participants=%d"), ParticipantCount);
			return;
		}

		// 참가 인원 전달과 맵 생성 요청은 분리한다. 새 Seed는 Maps가 생성한다.
		Coordinator->RequestGeneration();
	}

	// 완료 알림 외에도 퇴장으로 준비 조건이 풀리는 경우 재확인한다.
	GetWorldTimerManager().SetTimer(RoundMapPreparationTimerHandle, this, &ThisClass::TryFinishRoundMapPreparation, 0.1f, true);
}

bool AP48GameModeBase::IsRoundSpawnParticipant(const AP48PlayerState* Player) const
{
	return IsValid(Player) && Player->IsMatchParticipant();
}

void AP48GameModeBase::SetPlayerInputBlocked(APlayerController* PlayerController, bool bBlocked)
{
	AP48PlayerController* PC = Cast<AP48PlayerController>(PlayerController);
	if (!PC) { return; }
	if (AP48PlayerCharacter* Character = PC->GetPawn<AP48PlayerCharacter>())
	{
		Character->SetInputBlocked(bBlocked);
	}
	if (InputBlockedControllers.Contains(PC) == bBlocked) { return; }
	if (bBlocked) { InputBlockedControllers.Add(PC); }
	else { InputBlockedControllers.Remove(PC); }
	PC->Client_SetPlayInputBlocked(bBlocked);
}

void AP48GameModeBase::SetRoundInputBlocked(bool bBlocked)
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		// 관전 이동/시점은 플레이용 조작 차단 대상에서 제외한다.
		if (PC && PC->GetPawn<ASpectatorPawn>()) { continue; }
		const AP48PlayerState* Player = PC ? PC->GetPlayerState<AP48PlayerState>() : nullptr;
		SetPlayerInputBlocked(PC, bBlocked || !IsRoundSpawnParticipant(Player) || !Player->IsAlive());
	}
}

void AP48GameModeBase::StartPlayerSpectating(APlayerController* PlayerController)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = PlayerController;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASpectatorPawn* Pawn = GetWorld()->SpawnActor<ASpectatorPawn>(SpectatorClass,
		PlayerController->GetFocalLocation(), PlayerController->GetControlRotation(), SpawnParameters);
	if (!Pawn) { return; }
	SetPlayerInputBlocked(PlayerController, false);
	PlayerController->Possess(Pawn);
	PlayerController->PlayerState->SetIsSpectator(true);
}

void AP48GameModeBase::StopPlayerSpectating(APlayerController* PlayerController)
{
	if (ASpectatorPawn* Pawn = PlayerController->GetPawn<ASpectatorPawn>())
	{
		PlayerController->UnPossess();
		Pawn->Destroy();
		PlayerController->PlayerState->SetIsSpectator(false);
	}
}

bool AP48GameModeBase::RespawnRoundParticipants(
	const TArray<APlayerController*>& Participants,
	const int32 GenerationId)
{
	UP48PlayerStartRegistrySubsystem* Registry = GetWorld()->GetSubsystem<UP48PlayerStartRegistrySubsystem>();
	if (!Registry)
	{
		UE_LOG(LogTemp, Error, TEXT("[RoundSpawn] PlayerStart registry not found."));
		return false;
	}

	TArray<AP48PlayerStart*> PlayerStarts;
	if (!Registry->GetReadyPlayerStarts(GenerationId, PlayerStarts)
		|| PlayerStarts.Num() < Participants.Num())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[RoundSpawn] PlayerStarts are not ready. Generation=%d Starts=%d Participants=%d"),
			GenerationId, PlayerStarts.Num(), Participants.Num());
		return false;
	}

	// 스폰 입력과 사망 Pawn의 지연 삭제 완료 여부를 확인한다.
	for (int32 Index = 0; Index < Participants.Num(); ++Index)
	{
		APlayerController* PlayerController = Participants[Index];
		if (!IsValid(PlayerController)
			|| !IsValid(PlayerStarts[Index])
			|| !GetDefaultPawnClassForController(PlayerController))
		{
			UE_LOG(LogTemp, Error, TEXT("[RoundSpawn] Invalid respawn input at index %d."), Index);
			return false;
		}
		if (PlayerController->GetPawn() && !PlayerController->GetPawn<ASpectatorPawn>()
			&& !PlayerController->GetPlayerState<AP48PlayerState>()->IsAlive())
		{
			return false;
		}
	}

	for (int32 Index = 0; Index < Participants.Num(); ++Index)
	{
		APlayerController* PlayerController = Participants[Index];
		AP48PlayerStart* PlayerStart = PlayerStarts[Index];

		// 이전 시도에서 재생성에 성공한 참가자는 유지한다.
		StopPlayerSpectating(PlayerController);
		if (PlayerController->GetPawn()) { continue; }

		RestartPlayerAtPlayerStart(PlayerController, PlayerStart);
		if (!IsValid(PlayerController->GetPawn()))
		{
			UE_LOG(LogTemp, Error,
				TEXT("[RoundSpawn] Failed to respawn %s at slot %d."),
				*GetNameSafe(PlayerController), PlayerStart->SpawnSlotIndex);
			return false;
		}

		SetPlayerInputBlocked(PlayerController, true);
		// TODO(플레이어): PC의 차단 상태가 클라이언트의 새 Pawn에도 적용되도록 연결 필요.
		UE_LOG(LogTemp, Display,
			TEXT("[RoundSpawn] Respawned %s at slot %d. Generation=%d"),
			*GetNameSafe(PlayerController), PlayerStart->SpawnSlotIndex, GenerationId);
	}

	return true;
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

	if (P48MapReadiness::RequiresNetworkSeed(GetWorld()))
	{
		if (!RespawnRoundParticipants(Participants, SeedState->State.Revision)) { return; }
	}
	else
	{
		// PCG가 없는 테스트 맵도 Pawn 정리 후 기본 PlayerStart에서 재생성한다.
		for (APlayerController* PC : Participants)
		{
			if (PC->GetPawn() && !PC->GetPawn<ASpectatorPawn>()
				&& !PC->GetPlayerState<AP48PlayerState>()->IsAlive()) { return; }
			StopPlayerSpectating(PC);
			if (!PC->GetPawn())
			{
				RestartPlayer(PC);
			}
			if (!PC->GetPawn()) { return; }
			SetPlayerInputBlocked(PC, true);
		}
	}

	bWaitingForRoundMap = false;
	GetWorldTimerManager().ClearTimer(RoundMapPreparationTimerHandle);
	UE_LOG(LogTemp, Display, TEXT("[RoundMap] Ready and players respawned. Participants=%d"), Participants.Num());
	// 서버 재생성 완료 기준. 클라이언트의 새 Pawn 차단 유지 처리는 플레이어 담당과 연동한다.
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
