#include "P48LobbyGameMode.h"

#include "P48LobbyGameState.h"
#include "P48LobbyPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "Project48/Character/P48PlayerState.h"
#include "Project48/Online/P48MatchmakingSubsystem.h"

// TEMP: diagnostic output only; no passwords, member IDs or connection credentials.
DEFINE_LOG_CATEGORY_STATIC(LogP48LobbyRetention, Log, All);

namespace
{
bool IsValidLobbyTravelAddress(const FString& Address)
{
	FString Host;
	FString PortText;
	int32 Port = 0;
	return Address.Split(TEXT(":"), &Host, &PortText, ESearchCase::CaseSensitive,
		ESearchDir::FromEnd) && !Host.IsEmpty() && PortText.IsNumeric()
		&& LexTryParseString(Port, *PortText) && Port >= 1 && Port <= 65535
		&& !Address.Contains(TEXT("?")) && !Address.Contains(TEXT("/"))
		&& !Address.Contains(TEXT("\\")) && !Address.Contains(TEXT(" "));
}
}

void AP48LobbyGameMode::LogRoomRetentionSnapshot(const TCHAR* Event) const
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority()) return;
	UE_LOG(LogP48LobbyRetention, Display, TEXT("[LobbyRetention] Event=%s Rooms=%d"),
		Event, LobbyRooms.Num());
	for (const FP48LobbyRoomState& Room : LobbyRooms)
	{
		int32 LobbyConnected = 0;
		for (const AP48PlayerState* State : Room.Participants)
		{
			if (IsValid(State)) ++LobbyConnected;
		}
		int32 HostRecords = 0;
		for (const FP48LobbyTravelMember& Member : Room.TravelMembers)
		{
			if (Member.bIsHost) ++HostRecords;
		}
		UE_LOG(LogP48LobbyRetention, Display,
			TEXT("[LobbyRetention] Event=%s RoomId=%d LobbyConnected=%d TravelRecords=%d TotalMembers=%d HostRecords=%d HasGameServer=%d TravelRequested=%d"),
			Event, Room.RoomId, LobbyConnected, Room.TravelMembers.Num(), Room.GetMemberCount(),
			HostRecords, !Room.AssignedGameServerAddress.IsEmpty(), Room.bMatchStarting);
	}
#endif
}

void FP48LobbyRoomState::RecordGameHandoff(const FString& Destination)
{
	AssignedGameServerAddress = Destination;
	if (!MatchId.IsValid()) MatchId = FGuid::NewGuid();
	bMatchStarting = true;
	bGameSessionEnded = false;
	bGameServerReadyForReuse = false;
	for (AP48PlayerState* State : Participants)
	{
		RecordMemberHandoff(State);
	}
}

bool FP48LobbyRoomState::RecordMemberHandoff(AP48PlayerState* State)
{
	if (!IsValid(State) || TravelMembers.ContainsByPredicate(
		[State](const FP48LobbyTravelMember& Member) { return Member.LobbyPlayerState.Get() == State; }))
	{
		return false;
	}
	FP48LobbyTravelMember& Member = TravelMembers.AddDefaulted_GetRef();
	Member.MemberId = FGuid::NewGuid();
	//Member.PlayerName = State->GetPlayerName();
	Member.PlayerName = State->GetNickname();
	Member.bIsHost = State == HostPlayerState;
	Member.bAtGameServer = true;
	Member.LobbyPlayerState = State;
	return true;
}

const FP48LobbyTravelMember* FP48LobbyRoomState::FindTravelMember(
	const AP48PlayerState* State) const
{
	return TravelMembers.FindByPredicate([State](const FP48LobbyTravelMember& Member)
	{
		return Member.LobbyPlayerState.Get() == State;
	});
}

bool FP48LobbyRoomState::RestoreTravelMember(
	const FGuid& MemberId, AP48PlayerState* State)
{
	if (!MemberId.IsValid() || !IsValid(State)) return false;
	FP48LobbyTravelMember* Member = TravelMembers.FindByPredicate(
		[&MemberId](const FP48LobbyTravelMember& Candidate)
		{
			return Candidate.MemberId == MemberId && Candidate.bAtGameServer;
		});
	if (!Member) return false;

	Member->bAtGameServer = false;
	Member->LobbyPlayerState = State;
	//State->SetPlayerName(Member->PlayerName);
	State->SetNickname(Member->PlayerName);
	Participants.AddUnique(State);
	if (Member->bIsHost) HostPlayerState = State;
	return true;
}

bool FP48LobbyRoomState::AreAllTravelMembersBack() const
{
	return !TravelMembers.IsEmpty()
		&& !TravelMembers.ContainsByPredicate([](const FP48LobbyTravelMember& Member)
		{
			return Member.bAtGameServer;
		});
}

void FP48LobbyRoomState::RemoveParticipant(AP48PlayerState* State, bool bPreserveTravelRecord)
{
	if (!State) return;
	Participants.Remove(State);
	if (HostPlayerState == State) HostPlayerState = nullptr;
	if (bPreserveTravelRecord)
	{
		for (FP48LobbyTravelMember& Member : TravelMembers)
		{
			if (Member.LobbyPlayerState.Get() == State) Member.LobbyPlayerState.Reset();
		}
	}
	else
	{
		TravelMembers.RemoveAll([State](const FP48LobbyTravelMember& Member)
		{
			return Member.LobbyPlayerState.Get() == State;
		});
	}
}

int32 FP48LobbyRoomState::GetMemberCount() const
{
	int32 Count = TravelMembers.Num();
	for (const AP48PlayerState* State : Participants)
	{
		if (IsValid(State) && !TravelMembers.ContainsByPredicate(
			[State](const FP48LobbyTravelMember& Member) { return Member.LobbyPlayerState.Get() == State; }))
		{
			++Count;
		}
	}
	return Count;
}

AP48LobbyGameMode::AP48LobbyGameMode()
{
	GameStateClass = AP48LobbyGameState::StaticClass();
	PlayerControllerClass = AP48LobbyPlayerController::StaticClass();
	PlayerStateClass = AP48PlayerState::StaticClass();
	DefaultPawnClass = nullptr;
	bUseSeamlessTravel = false;
}

void AP48LobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	FParse::Value(FCommandLine::Get(), TEXT("LobbyGameServerAddress="), GameServerAddress);
	FString GameServerPoolOption;
	if (FParse::Value(FCommandLine::Get(), TEXT("LobbyGameServerAddresses="),
		GameServerPoolOption))
	{
		GameServerAddresses.Reset();
		GameServerPoolOption.ParseIntoArray(GameServerAddresses, TEXT(","), true);
	}
	FParse::Value(FCommandLine::Get(), TEXT("LobbyReturnAddress="), LobbyReturnAddress);
	FParse::Value(FCommandLine::Get(), TEXT("LobbyReturnGracePeriod="),
		LobbyReturnGracePeriodSeconds);
	LobbyReturnGracePeriodSeconds = FMath::Max(5.0f, LobbyReturnGracePeriodSeconds);

	if (GetNetMode() != NM_DedicatedServer || bCreateSessionOnDedicatedServer == false)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UP48MatchmakingSubsystem* MatchmakingSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UP48MatchmakingSubsystem>() : nullptr;

	if (IsValid(MatchmakingSubsystem)
		&& MatchmakingSubsystem->HasActiveSession() == false)
	{
		MatchmakingSubsystem->CreateSession(MaxLobbyPlayers * MaxLobbyRooms, true);
	}
}

FString AP48LobbyGameMode::InitNewPlayer(APlayerController* NewPlayerController,
	const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString SuperError = Super::InitNewPlayer(
		NewPlayerController, UniqueId, Options, Portal);
	if (!SuperError.IsEmpty()) return SuperError;

	// The main menu sends the display name as a travel option. Keep the
	// canonical value in APlayerState::PlayerName so lobby and game UI can use
	// the same replicated field without depending on a menu-specific subclass.
	const FString Nickname = UGameplayStatics::ParseOption(
		Options, TEXT("Nickname")).TrimStartAndEnd();
	if (!Nickname.IsEmpty())
	{
		ChangeName(NewPlayerController, Nickname, false);
		
		AP48PlayerState* P48PS = NewPlayerController->GetPlayerState<AP48PlayerState>();
		if (IsValid(P48PS) == false)
		{
			UE_LOG(LogTemp, Error, TEXT("P48PS 생성 실패 로비"));
			return "";
		}
		P48PS->SetNickname(Nickname);
	}

	const FString RoomOption = UGameplayStatics::ParseOption(Options, TEXT("LobbyRoomId"));
	const FString MemberOption = UGameplayStatics::ParseOption(Options, TEXT("LobbyMemberId"));
	if (RoomOption.IsEmpty() && MemberOption.IsEmpty()) return FString();

	int32 RoomId = INDEX_NONE;
	FGuid MemberId;
	if (!LexTryParseString(RoomId, *RoomOption) || RoomId < 1
		|| !FGuid::Parse(MemberOption, MemberId))
	{
		return TEXT("Invalid lobby return ticket.");
	}

	const FP48LobbyRoomState* Room = FindRoom(RoomId);
	const bool bValidMember = Room && Room->TravelMembers.ContainsByPredicate(
		[&MemberId](const FP48LobbyTravelMember& Member)
		{
			return Member.MemberId == MemberId && Member.bAtGameServer;
		});
	AP48LobbyPlayerController* LobbyController =
		Cast<AP48LobbyPlayerController>(NewPlayerController);
	if (!bValidMember || !IsValid(LobbyController))
	{
		return TEXT("Lobby return ticket is expired or does not match a room.");
	}

	LobbyController->SetPendingLobbyReturn(RoomId, MemberId);
	return FString();
}

void AP48LobbyGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);

	AP48PlayerState* JoinedPlayerState =
		NewPlayer ? NewPlayer->GetPlayerState<AP48PlayerState>() : nullptr;
	if (IsValid(JoinedPlayerState) == false)
	{
		return;
	}
	AP48LobbyPlayerController* LobbyController =
		Cast<AP48LobbyPlayerController>(NewPlayer);
	if (IsValid(LobbyController)
		&& RestoreReturningPlayer(LobbyController, JoinedPlayerState))
	{
		return;
	}

	JoinedPlayerState->SetReady(false);
	if (IsValid(LobbyController))
	{
		LobbyController->SetLobbyRoomId(INDEX_NONE);
	}

	RefreshLobbyState();
}

bool AP48LobbyGameMode::RestoreReturningPlayer(
	AP48LobbyPlayerController* PlayerController, AP48PlayerState* PlayerState)
{
	int32 RoomId = INDEX_NONE;
	FGuid MemberId;
	if (!PlayerController->ConsumePendingLobbyReturn(RoomId, MemberId)) return false;

	FP48LobbyRoomState* Room = FindRoom(RoomId);
	if (!Room || !Room->RestoreTravelMember(MemberId, PlayerState)) return false;

	const bool bWasAlreadyReturning = Room->bReturningToLobby;
	Room->bReturningToLobby = true;
	PlayerState->SetReady(false);
	PlayerController->SetLobbyRoomId(RoomId);
	if (Room->bGameSessionEnded && Room->AreAllTravelMembersBack())
	{
		FinalizeLobbyReturn(*Room, false);
		if (Room->Participants.Num() <= 1) CloseGameRoom(RoomId);
	}
	else if (!bWasAlreadyReturning)
	{
		StartLobbyReturnGracePeriod(RoomId);
	}

	RefreshLobbyState();
	LogRoomRetentionSnapshot(TEXT("PlayerReturned"));
	PlayerController->Client_ConfirmLobbyReturn();
	return true;
}

void AP48LobbyGameMode::StartLobbyReturnGracePeriod(int32 RoomId)
{
	FTimerHandle& TimerHandle = LobbyReturnTimers.FindOrAdd(RoomId);
	FTimerDelegate TimeoutDelegate = FTimerDelegate::CreateUObject(
		this, &AP48LobbyGameMode::HandleLobbyReturnTimeout, RoomId);
	GetWorldTimerManager().SetTimer(TimerHandle, TimeoutDelegate,
		FMath::Max(5.0f, LobbyReturnGracePeriodSeconds), false);
}

void AP48LobbyGameMode::HandleLobbyReturnTimeout(int32 RoomId)
{
	FP48LobbyRoomState* Room = FindRoom(RoomId);
	if (!Room || !Room->bReturningToLobby)
	{
		LobbyReturnTimers.Remove(RoomId);
		return;
	}

	const bool bCloseRoom = Room->bGameSessionEnded && Room->Participants.Num() <= 1;
	FinalizeLobbyReturn(*Room, true);
	if (bCloseRoom) CloseGameRoom(RoomId);
	else RefreshLobbyState();
	LogRoomRetentionSnapshot(TEXT("ReturnTimeoutFinalized"));
}

void AP48LobbyGameMode::FinalizeLobbyReturn(
	FP48LobbyRoomState& Room, bool bDiscardMissingMembers)
{
	if (FTimerHandle* TimerHandle = LobbyReturnTimers.Find(Room.RoomId))
	{
		GetWorldTimerManager().ClearTimer(*TimerHandle);
	}
	LobbyReturnTimers.Remove(Room.RoomId);

	int32 MissingMemberCount = 0;
	if (bDiscardMissingMembers)
	{
		for (const FP48LobbyTravelMember& Member : Room.TravelMembers)
		{
			if (Member.bAtGameServer) ++MissingMemberCount;
		}
	}
	if (MissingMemberCount > 0)
	{
		UE_LOG(LogP48LobbyRetention, Warning,
			TEXT("[LobbyRetention] Return grace period expired for RoomId=%d; releasing %d missing member record(s)."),
			Room.RoomId, MissingMemberCount);
	}
	Room.bMatchStarting = false;
	Room.bReturningToLobby = false;
	if (Room.bGameServerReadyForReuse)
	{
		Room.AssignedGameServerAddress.Reset();
		Room.MatchId.Invalidate();
		Room.bGameServerReadyForReuse = false;
	}
	Room.TravelMembers.Reset();
	for (AP48PlayerState* Participant : Room.Participants)
	{
		if (IsValid(Participant)) Participant->SetReady(false);
	}
}

bool AP48LobbyGameMode::ReportGameSessionEnded(int32 RoomId,
	const FGuid& ReportedMatchId)
{
	if (!HasAuthority() || !ReportedMatchId.IsValid()) return false;
	FP48LobbyRoomState* Room = FindRoom(RoomId);
	if (!Room || Room->MatchId != ReportedMatchId
		|| Room->AssignedGameServerAddress.IsEmpty())
	{
		return false;
	}
	if (Room->bGameSessionEnded) return true;
	Room->bGameSessionEnded = true;
	Room->bReturningToLobby = true;
	if (Room->TravelMembers.IsEmpty() || Room->AreAllTravelMembersBack())
	{
		FinalizeLobbyReturn(*Room, false);
		if (Room->Participants.Num() <= 1) CloseGameRoom(RoomId);
		else RefreshLobbyState();
	}
	else
	{
		StartLobbyReturnGracePeriod(RoomId);
		RefreshLobbyState();
	}
	LogRoomRetentionSnapshot(TEXT("GameSessionEnded"));
	return true;
}

bool AP48LobbyGameMode::ReportGameServerReady(int32 RoomId, const FGuid& ReportedMatchId)
{
	if (!HasAuthority() || !ReportedMatchId.IsValid()) return false;
	FP48LobbyRoomState* Room = FindRoom(RoomId);
	if (!Room || Room->MatchId != ReportedMatchId)
	{
		const FP48PendingGameServerReset* Pending =
			PendingGameServerResets.Find(ReportedMatchId);
		if (!Pending || Pending->RoomId != RoomId) return false;
		PendingGameServerResets.Remove(ReportedMatchId);
		return true;
	}
	if (Room->AssignedGameServerAddress.IsEmpty()) return false;
	Room->bGameServerReadyForReuse = true;
	if (!Room->bMatchStarting && !Room->bReturningToLobby && Room->bGameSessionEnded)
	{
		Room->AssignedGameServerAddress.Reset();
		Room->MatchId.Invalidate();
		Room->bGameServerReadyForReuse = false;
	}
	RefreshLobbyState();
	return true;
}

void AP48LobbyGameMode::CloseGameRoom(int32 RoomId)
{
	FP48LobbyRoomState* Room = FindRoom(RoomId);
	if (!Room) return;
	if (!Room->bGameServerReadyForReuse && Room->MatchId.IsValid()
		&& !Room->AssignedGameServerAddress.IsEmpty())
	{
		FP48PendingGameServerReset& Pending = PendingGameServerResets.FindOrAdd(Room->MatchId);
		Pending.RoomId = RoomId;
		Pending.ServerAddress = Room->AssignedGameServerAddress;
	}
	if (FTimerHandle* TimerHandle = LobbyReturnTimers.Find(RoomId))
	{
		GetWorldTimerManager().ClearTimer(*TimerHandle);
	}
	LobbyReturnTimers.Remove(RoomId);
	for (AP48PlayerState* Participant : Room->Participants)
	{
		if (!IsValid(Participant)) continue;
		Participant->SetReady(false);
		if (AP48LobbyPlayerController* Controller =
			Cast<AP48LobbyPlayerController>(Participant->GetOwner()))
		{
			Controller->SetLobbyRoomId(INDEX_NONE);
		}
	}
	LobbyRooms.RemoveAll([RoomId](const FP48LobbyRoomState& Candidate)
	{
		return Candidate.RoomId == RoomId;
	});
	RefreshLobbyState();
}

void AP48LobbyGameMode::Logout(AController* Exiting)
{
	AP48PlayerState* ExitingPlayerState =
		Exiting ? Exiting->GetPlayerState<AP48PlayerState>() : nullptr;
	// Preserve only pre-recorded handoffs. Ordinary disconnects have no travel record.
	RemoveLobbyParticipant(ExitingPlayerState, true);
	Super::Logout(Exiting);
	RefreshLobbyState();
	LogRoomRetentionSnapshot(TEXT("AfterLogout"));
}

bool AP48LobbyGameMode::IsLobbyParticipant(
	const AP48LobbyPlayerController* PlayerController) const
{
	const AP48PlayerState* State = IsValid(PlayerController)
		? PlayerController->GetPlayerState<AP48PlayerState>() : nullptr;
	return IsValid(State) && FindRoomForPlayer(State) != nullptr;
}

bool AP48LobbyGameMode::RequestCreateLobby(
	AP48LobbyPlayerController* PlayerController, bool bUsePassword,
	const FString& Password, FText& OutError)

{
	return RequestCreateLobbyWithSettings(
		PlayerController, FString(), bUsePassword, Password, OutError);
}

bool AP48LobbyGameMode::RequestCreateLobbyWithSettings(
	AP48LobbyPlayerController* PlayerController, const FString& RoomTitle,
	bool bUsePassword, const FString& Password, FText& OutError)
{
	OutError = FText::GetEmpty();
	AP48PlayerState* State = IsValid(PlayerController)
		? PlayerController->GetPlayerState<AP48PlayerState>() : nullptr;
	if (!HasAuthority() || !IsValid(State))
	{
		OutError = FText::FromString(TEXT("Player is not ready to create a room."));
		return false;
	}
	if (IsLobbyParticipant(PlayerController))
	{
		return true;
	}
	if (LobbyRooms.Num() >= MaxLobbyRooms)
	{
		OutError = FText::FromString(TEXT("The maximum number of rooms has been reached."));
		return false;
	}
	bool bPasswordContainsOnlyDigits = true;
	for (const TCHAR Character : Password)
	{
		if (!FChar::IsDigit(Character))
		{
			bPasswordContainsOnlyDigits = false;
			break;
		}
	}
	if (bUsePassword
		&& (Password.IsEmpty() || Password.Len() > 4
			|| !bPasswordContainsOnlyDigits))
	{
		OutError = FText::FromString(TEXT("The room password must contain 1 to 4 digits."));
		return false;
	}
	const FString TrimmedTitle = RoomTitle.TrimStartAndEnd();
	if (TrimmedTitle.Len() > 24 || TrimmedTitle.Contains(TEXT("\n"))
		|| TrimmedTitle.Contains(TEXT("\r")) || TrimmedTitle.Contains(TEXT("\t")))
	{
		OutError = FText::FromString(
			TEXT("The room title must be a single line with at most 24 characters."));
		return false;
	}

	int32 NewRoomId = 1;
	while (FindRoom(NewRoomId) != nullptr)
	{
		++NewRoomId;
	}
	FP48LobbyRoomState& NewRoom = LobbyRooms.AddDefaulted_GetRef();
	NewRoom.RoomId = NewRoomId;
	NewRoom.RoomTitle = TrimmedTitle.IsEmpty()
		? FString::Printf(TEXT("Room %d"), NewRoomId) : TrimmedTitle;
	NewRoom.Participants.Add(State);
	NewRoom.HostPlayerState = State;
	NewRoom.Password = bUsePassword ? Password : FString();
	State->SetReady(false);
	RefreshLobbyState();
	PlayerController->SetLobbyRoomId(NewRoomId);
	LogRoomRetentionSnapshot(TEXT("RoomCreated"));
	return true;
}

bool AP48LobbyGameMode::RequestJoinLobby(
	AP48LobbyPlayerController* PlayerController, FText& OutError)
{
	OutError = FText::GetEmpty();
	AP48PlayerState* State = IsValid(PlayerController)
		? PlayerController->GetPlayerState<AP48PlayerState>() : nullptr;
	if (!HasAuthority() || !IsValid(State))
	{
		OutError = FText::FromString(TEXT("Player is not ready to join."));
		return false;
	}
	for (FP48LobbyRoomState& Room : LobbyRooms)
	{
		// Room admission remains open after game handoff; spectator travel is a separate request.
		if (Room.Password.IsEmpty()
			&& Room.GetMemberCount() < MaxLobbyPlayers)
		{
			return RequestJoinLobbyById(PlayerController, Room.RoomId, FString(), OutError);
		}
	}
	OutError = LobbyRooms.IsEmpty()
		? FText::FromString(TEXT("There is no open room."))
		: FText::FromString(TEXT("Select a room or enter its password."));
	return false;
}

bool AP48LobbyGameMode::RequestJoinLobbyById(
	AP48LobbyPlayerController* PlayerController, int32 RoomId,
	const FString& Password, FText& OutError)
{
	OutError = FText::GetEmpty();
	AP48PlayerState* State = IsValid(PlayerController)
		? PlayerController->GetPlayerState<AP48PlayerState>() : nullptr;
	if (!HasAuthority() || !IsValid(State))
	{
		OutError = FText::FromString(TEXT("Player is not ready to join."));
		return false;
	}
	if (IsLobbyParticipant(PlayerController))
	{
		return true;
	}
	FP48LobbyRoomState* Room = FindRoom(RoomId);
	if (!Room)
	{
		OutError = FText::FromString(TEXT("The selected room does not exist."));
		return false;
	}
	if (Room->bGameSessionEnded && Room->bReturningToLobby)
	{
		OutError = FText::FromString(TEXT("This room is closing after the match."));
		return false;
	}
	// Count members already sent to the game as well as those still in the lobby.
	if (Room->GetMemberCount() >= MaxLobbyPlayers)
	{
		OutError = FText::FromString(TEXT("The lobby is full."));
		return false;
	}
	if (!Room->Password.IsEmpty() && Room->Password != Password)
	{
		OutError = FText::FromString(TEXT("The room password is incorrect."));
		return false;
	}
	State->SetReady(false);
	Room->Participants.Add(State);
	RefreshLobbyState();
	PlayerController->SetLobbyRoomId(RoomId);
	LogRoomRetentionSnapshot(TEXT("RoomJoined"));
	return true;
}

void AP48LobbyGameMode::LeaveLobby(AP48LobbyPlayerController* PlayerController)
{
	if (!HasAuthority() || !IsValid(PlayerController))
	{
		return;
	}
	RemoveLobbyParticipant(PlayerController->GetPlayerState<AP48PlayerState>());
	RefreshLobbyState();
	PlayerController->SetLobbyRoomId(INDEX_NONE);
	LogRoomRetentionSnapshot(TEXT("ExplicitLeave"));
}

void AP48LobbyGameMode::RemoveLobbyParticipant(AP48PlayerState* State, bool bPreserveTravelRecord)
{
	if (IsValid(State))
	{
		State->SetReady(false);
	}
	for (FP48LobbyRoomState& Room : LobbyRooms)
	{
		Room.RemoveParticipant(State, bPreserveTravelRecord);
	}
	LobbyRooms.RemoveAll([](const FP48LobbyRoomState& Room)
	{
		return Room.IsEmpty();
	});
}

bool AP48LobbyGameMode::CanPlayerChangeReady(
	const AP48LobbyPlayerController* PlayerController) const
{
	const AP48PlayerState* State = IsValid(PlayerController)
		? PlayerController->GetPlayerState<AP48PlayerState>() : nullptr;
	const FP48LobbyRoomState* Room = FindRoomForPlayer(State);
	return Room && !Room->bReturningToLobby
		&& IsHostController(PlayerController) == false
		&& !Room->TravelMembers.ContainsByPredicate([State](const FP48LobbyTravelMember& Member)
		{
			return Member.bAtGameServer && Member.LobbyPlayerState.Get() == State;
		});
}

void AP48LobbyGameMode::RequestLobbyReady(
	AP48LobbyPlayerController* PlayerController, bool bNewReady)
{
	if (!HasAuthority()) return;
	AP48PlayerState* State = PlayerController->GetPlayerState<AP48PlayerState>();
	FP48LobbyRoomState* Room = FindRoom(PlayerController->GetLobbyRoomId());
	if (!IsValid(State) || !Room) return;
	if (!CanPlayerChangeReady(PlayerController))
	{
		if (Room->bReturningToLobby)
		{
			PlayerController->Client_ReportLobbyRequestFailure(FText::FromString(
				TEXT("Ready is unavailable while game players are returning.")));
		}
		return;
	}

	State->SetReady(bNewReady);
	if (!bNewReady || !Room->bMatchStarting || Room->AssignedGameServerAddress.IsEmpty())
	{
		RefreshLobbyState();
		return;
	}

	const FString Destination = Room->AssignedGameServerAddress;
	const FString ReturnDestination = LobbyReturnAddress.TrimStartAndEnd();
	if (!IsValidLobbyTravelAddress(ReturnDestination))
	{
		PlayerController->Client_ReportLobbyRequestFailure(
			FText::FromString(TEXT("The lobby return address is not configured.")));
		return;
	}
	if (!Room->RecordMemberHandoff(State))
	{
		PlayerController->Client_ReportLobbyRequestFailure(
			FText::FromString(TEXT("The spectator travel request is already being processed.")));
		return;
	}

	const FP48LobbyTravelMember* Member = Room->FindTravelMember(State);
	if (!Member)
	{
		PlayerController->Client_ReportLobbyRequestFailure(
			FText::FromString(TEXT("Could not issue a lobby return ticket.")));
		return;
	}
	const int32 ReturnRoomId = Room->RoomId;
	const FGuid ReturnMemberId = Member->MemberId;
	RefreshLobbyState();
	LogRoomRetentionSnapshot(TEXT("LateJoinHandoffRecorded"));
	PlayerController->Client_TravelToGameServer(Destination, ReturnDestination,
		ReturnRoomId, Room->MatchId, ReturnMemberId, 0);
}

void AP48LobbyGameMode::NotifyLobbyReadyStateChanged()
{
	RefreshLobbyState();
}

void AP48LobbyGameMode::RequestStartGame(
	AP48LobbyPlayerController* RequestingController)
{
	if (!HasAuthority())
	{
		return;
	}
	AP48PlayerState* RequestingState = IsValid(RequestingController)
		? RequestingController->GetPlayerState<AP48PlayerState>() : nullptr;
	const FP48LobbyRoomState* Room = FindRoomForPlayer(RequestingState);
	if (!Room || Room->bMatchStarting || IsHostController(RequestingController) == false)
	{
		if (IsValid(RequestingController))
		{
			RequestingController->Client_ReportLobbyRequestFailure(
				FText::FromString(TEXT("Only the lobby host can start the match.")));
		}
		return;
	}
	if (!Room->AssignedGameServerAddress.IsEmpty())
	{
		RequestingController->Client_ReportLobbyRequestFailure(
			FText::FromString(TEXT("The game server is still resetting.")));
		return;
	}

	RefreshLobbyState();

	const AP48LobbyGameState* LobbyGameState = GetGameState<AP48LobbyGameState>();
	if (IsValid(LobbyGameState) == false
		|| LobbyGameState->CanPlayerStartGame(RequestingController->PlayerState) == false)
	{
		RequestingController->Client_ReportLobbyRequestFailure(
			FText::FromString(TEXT("All guests must be ready before starting.")));
		return;
	}

	const FString Destination = FindAvailableGameServerAddress();
	const FString ReturnDestination = LobbyReturnAddress.TrimStartAndEnd();
	if (Destination.IsEmpty())
	{
		RequestingController->Client_ReportLobbyRequestFailure(FText::FromString(
			TEXT("No configured game server is currently available.")));
		return;
	}
	if (!IsValidLobbyTravelAddress(ReturnDestination))
	{
		RequestingController->Client_ReportLobbyRequestFailure(FText::FromString(
			TEXT("Configure Lobby Return Address as host:port before starting.")));
		return;
	}

	// RefreshLobbyState can remove entries: reacquire instead of retaining the old room pointer.
	FP48LobbyRoomState* StartingRoom = FindRoom(RequestingController->GetLobbyRoomId());
	if (!StartingRoom || StartingRoom->HostPlayerState != RequestingState)
	{
		return;
	}

	TArray<AP48LobbyPlayerController*> TravelingPlayers;
	for (AP48PlayerState* Participant : StartingRoom->Participants)
	{
		AP48LobbyPlayerController* Controller = IsValid(Participant)
			? Cast<AP48LobbyPlayerController>(Participant->GetOwner()) : nullptr;
		if (!IsValid(Controller) || Controller->GetPlayerState<AP48PlayerState>() != Participant)
		{
			RequestingController->Client_ReportLobbyRequestFailure(FText::FromString(
				TEXT("A room participant is disconnecting. Try again after the roster updates.")));
			return;
		}
		TravelingPlayers.Add(Controller);
	}

	// Lock before dispatching any RPC. Menu users and other rooms are never enumerated here.
	LogRoomRetentionSnapshot(TEXT("BeforeHandoff"));
	StartingRoom->RecordGameHandoff(Destination);
	const int32 ReturnRoomId = StartingRoom->RoomId;
	TArray<FGuid> ReturnMemberIds;
	ReturnMemberIds.Reserve(TravelingPlayers.Num());
	for (AP48LobbyPlayerController* Controller : TravelingPlayers)
	{
		const FP48LobbyTravelMember* Member = StartingRoom->FindTravelMember(
			Controller->GetPlayerState<AP48PlayerState>());
		if (!Member)
		{
			RequestingController->Client_ReportLobbyRequestFailure(
				FText::FromString(TEXT("Could not issue lobby return tickets.")));
			return;
		}
		ReturnMemberIds.Add(Member->MemberId);
	}
	RefreshLobbyState();
	LogRoomRetentionSnapshot(TEXT("HandoffRecorded"));
	for (int32 Index = 0; Index < TravelingPlayers.Num(); ++Index)
	{
		TravelingPlayers[Index]->Client_TravelToGameServer(Destination,
			ReturnDestination, ReturnRoomId, StartingRoom->MatchId,
			ReturnMemberIds[Index], TravelingPlayers.Num());
	}
}

void AP48LobbyGameMode::RefreshLobbyState()
{
	AP48LobbyGameState* LobbyGameState =
		GetGameState<AP48LobbyGameState>();
	if (IsValid(LobbyGameState) == false)
	{
		return;
	}

	for (FP48LobbyRoomState& Room : LobbyRooms)
	{
		Room.Participants.RemoveAll([](const TObjectPtr<AP48PlayerState>& State)
		{
			return !IsValid(State.Get());
		});
		// A host traveling to the game has not relinquished ownership of the room.
		if (!Room.bMatchStarting && (!IsValid(Room.HostPlayerState)
			|| !Room.Participants.Contains(Room.HostPlayerState)))
		{
			Room.HostPlayerState = nullptr;
			for (AP48PlayerState* Candidate : Room.Participants)
			{
				if (IsValid(Candidate))
				{
					Room.HostPlayerState = Candidate;
					Candidate->SetReady(false);
					break;
				}
			}
		}
	}
	LobbyRooms.RemoveAll([](const FP48LobbyRoomState& Room)
	{
		return Room.IsEmpty();
	});

	TArray<FP48LobbyPlayerEntry> Entries;
	TArray<FP48LobbyRoomInfo> RoomInfos;
	for (const FP48LobbyRoomState& Room : LobbyRooms)
	{
		FP48LobbyRoomInfo& RoomInfo = RoomInfos.AddDefaulted_GetRef();
		RoomInfo.RoomId = Room.RoomId;
		RoomInfo.RoomTitle = Room.RoomTitle;
		RoomInfo.CurrentPlayers = Room.GetMemberCount();
		RoomInfo.MaxPlayers = MaxLobbyPlayers;
		RoomInfo.bRequiresPassword = !Room.Password.IsEmpty();
		RoomInfo.bHasGameServer = Room.bMatchStarting && !Room.AssignedGameServerAddress.IsEmpty();
		RoomInfo.bIsReturningToLobby = Room.bReturningToLobby;
		RoomInfo.bIsGameServerResetting = !Room.bMatchStarting
			&& !Room.AssignedGameServerAddress.IsEmpty();

		for (const FP48LobbyTravelMember& Member : Room.TravelMembers)
		{
			FP48LobbyPlayerEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.PlayerState = Member.LobbyPlayerState.Get();
			Entry.RoomId = Room.RoomId;
			Entry.PlayerName = Member.PlayerName;
			Entry.bIsHost = Member.bIsHost;
			Entry.bTravelRequested = Member.bAtGameServer;
			Entry.bIsReady = false;
		}

		for (AP48PlayerState* PlayerState : Room.Participants)
		{
			if (!IsValid(PlayerState)) continue;
			if (Room.TravelMembers.ContainsByPredicate([PlayerState](const FP48LobbyTravelMember& Member)
			{
				return Member.LobbyPlayerState.Get() == PlayerState;
			})) continue;
			FP48LobbyPlayerEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.PlayerState = PlayerState;
			Entry.RoomId = Room.RoomId;
			// Entry.PlayerName = PlayerState->GetPlayerName();
			Entry.PlayerName = PlayerState->GetNickname();
			Entry.bIsHost = PlayerState == Room.HostPlayerState;
			Entry.bIsReady = !Entry.bIsHost && PlayerState->IsReady();
		}
	}
	LobbyGameState->RebuildLobbyState(Entries, RoomInfos);
}

FP48LobbyRoomState* AP48LobbyGameMode::FindRoom(int32 RoomId)
{
	return LobbyRooms.FindByPredicate([RoomId](const FP48LobbyRoomState& Room)
	{
		return Room.RoomId == RoomId;
	});
}

const FP48LobbyRoomState* AP48LobbyGameMode::FindRoomForPlayer(
	const AP48PlayerState* PlayerState) const
{
	return LobbyRooms.FindByPredicate([PlayerState](const FP48LobbyRoomState& Room)
	{
		return Room.Participants.Contains(PlayerState);
	});
}

bool AP48LobbyGameMode::IsHostController(
	const AP48LobbyPlayerController* PlayerController) const
{
	const AP48PlayerState* State = IsValid(PlayerController)
		? PlayerController->GetPlayerState<AP48PlayerState>() : nullptr;
	const FP48LobbyRoomState* Room = FindRoomForPlayer(State);
	return Room && Room->HostPlayerState == State;
}

FString AP48LobbyGameMode::FindAvailableGameServerAddress() const
{
	TArray<FString> CandidateAddresses = GameServerAddresses;
	if (CandidateAddresses.IsEmpty() && !GameServerAddress.IsEmpty())
	{
		CandidateAddresses.Add(GameServerAddress);
	}

	for (FString Candidate : CandidateAddresses)
	{
		Candidate = Candidate.TrimStartAndEnd();
		if (!IsValidLobbyTravelAddress(Candidate)) continue;
		const bool bAlreadyAssigned = LobbyRooms.ContainsByPredicate(
			[&Candidate](const FP48LobbyRoomState& Room)
			{
				return Room.AssignedGameServerAddress.Equals(
					Candidate, ESearchCase::IgnoreCase);
			});
		bool bPendingReset = false;
		for (const TPair<FGuid, FP48PendingGameServerReset>& Pending : PendingGameServerResets)
		{
			if (Pending.Value.ServerAddress.Equals(Candidate, ESearchCase::IgnoreCase))
			{
				bPendingReset = true;
				break;
			}
		}
		if (!bAlreadyAssigned && !bPendingReset) return Candidate;
	}
	return FString();
}
