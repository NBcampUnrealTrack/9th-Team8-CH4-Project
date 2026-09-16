#include "P48LobbyPlayerController.h"

#include "P48LobbyGameMode.h"
#include "P48LobbyGameState.h"
#include "P48LobbyTravelSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Net/UnrealNetwork.h"
#include "Project48/Character/P48PlayerState.h"

namespace P48LobbyPlayerController
{
	const TCHAR* MainMenuMapPath = TEXT("/Game/PHS/MainMenu/HSLevel");
}

void AP48LobbyPlayerController::InitializeLobbyScreens(
	TSubclassOf<UUserWidget> MenuScreen, TSubclassOf<UUserWidget> LobbyScreen)
{
	if (!IsLocalController())
	{
		return;
	}
	if (!MenuScreen || !LobbyScreen)
	{
		OnLobbyRequestFailed.Broadcast(FText::FromString(TEXT("Both lobby screen classes must be configured.")));
		return;
	}
	MenuScreenClass = MenuScreen;
	LobbyScreenClass = LobbyScreen;
	// Initial false membership may not trigger RepNotify, so refresh explicitly.
	RefreshLobbyScreen();
}

void AP48LobbyPlayerController::RefreshLobbyScreen()
{
	if (!IsLocalController() || !MenuScreenClass || !LobbyScreenClass)
	{
		return;
	}
	const TSubclassOf<UUserWidget> DesiredClass = IsInLobby() ? LobbyScreenClass : MenuScreenClass;
	if (!IsValid(ActiveLobbyScreen) || ActiveLobbyScreen->GetClass() != DesiredClass.Get())
	{
		UUserWidget* NewScreen = CreateWidget<UUserWidget>(this, DesiredClass);
		if (!IsValid(NewScreen))
		{
			OnLobbyRequestFailed.Broadcast(FText::FromString(TEXT("Failed to create the lobby screen.")));
			return;
		}
		if (IsValid(ActiveLobbyScreen))
		{
			ActiveLobbyScreen->RemoveFromParent();
		}
		ActiveLobbyScreen = NewScreen;
	}
	if (!ActiveLobbyScreen->IsInViewport())
	{
		ActiveLobbyScreen->AddToPlayerScreen();
	}
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(ActiveLobbyScreen->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void AP48LobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(ActiveLobbyScreen))
	{
		ActiveLobbyScreen->RemoveFromParent();
		ActiveLobbyScreen = nullptr;
	}
	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
	Super::EndPlay(EndPlayReason);
}

void AP48LobbyPlayerController::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AP48LobbyPlayerController, LobbyRoomId, COND_OwnerOnly);
}

void AP48LobbyPlayerController::SetLobbyRoomId(int32 NewRoomId)
{
	if (!HasAuthority() || LobbyRoomId == NewRoomId)
	{
		return;
	}
	LobbyRoomId = NewRoomId;
	ForceNetUpdate();
	if (IsLocalController())
	{
		OnRep_LobbyMembership();
	}
}

void AP48LobbyPlayerController::OnRep_LobbyMembership()
{
	RefreshLobbyScreen();
	OnLobbyMembershipChanged.Broadcast(IsInLobby());
}

void AP48LobbyPlayerController::Client_ReportLobbyRequestFailure_Implementation(const FText& Reason)
{
	OnLobbyRequestFailed.Broadcast(Reason);
}

void AP48LobbyPlayerController::Client_TravelToGameServer_Implementation(
	const FString& ServerAddress, const FString& ReturnAddress,
	int32 ReturnRoomId, const FGuid& MatchId, const FGuid& ReturnMemberId,
	int32 ExpectedPlayers)
{
	if (!IsLocalController() || !MatchId.IsValid())
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UP48LobbyTravelSubsystem* TravelSubsystem = GameInstance
		? GameInstance->GetSubsystem<UP48LobbyTravelSubsystem>() : nullptr;
	if (!IsValid(TravelSubsystem)
		|| !TravelSubsystem->StoreLobbyReturnContext(
			ReturnAddress, ReturnRoomId, ReturnMemberId))
	{
		OnLobbyRequestFailed.Broadcast(
			FText::FromString(TEXT("Could not save the lobby return information.")));
		return;
	}
	// Keep the lobby widget until EndPlay so a failed travel can still display the lobby.
	// Release UI-only input before opening the remote game world.
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	FString Destination = ServerAddress;
	// Correlation data for the game server's eventual match-end report. These
	// client-visible values are not proof that the report came from a server.
	Destination += FString::Printf(TEXT("?LobbyRoomId=%d?LobbyMatchId=%s?LobbyMemberId=%s"),
		ReturnRoomId, *MatchId.ToString(EGuidFormats::Digits),
		*ReturnMemberId.ToString(EGuidFormats::Digits));
	if (ExpectedPlayers > 0)
	{
		Destination += FString::Printf(TEXT("?ExpectedPlayers=%d"), ExpectedPlayers);
	}
	if (const APlayerState* LocalPlayerState = PlayerState)
	{
		const FString Nickname = LocalPlayerState->GetPlayerName().TrimStartAndEnd();
		if (!Nickname.IsEmpty())
		{
			// Name is handled automatically by AGameModeBase and becomes the
			// replicated APlayerState::PlayerName. Keep Nickname as a compatibility
			// option for the existing HS login flow.
			Destination += FString::Printf(
				TEXT("?Name=%s?Nickname=%s"), *Nickname, *Nickname);
		}
	}
	if (!TravelSubsystem->BeginGameServerTravel(Destination))
	{
		OnLobbyRequestFailed.Broadcast(
			FText::FromString(TEXT("Could not start the game server connection.")));
	}
}

void AP48LobbyPlayerController::Client_ConfirmLobbyReturn_Implementation()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (UP48LobbyTravelSubsystem* TravelSubsystem = GameInstance
		? GameInstance->GetSubsystem<UP48LobbyTravelSubsystem>() : nullptr)
	{
		TravelSubsystem->ClearLobbyReturnContext();
	}
}

void AP48LobbyPlayerController::Client_ReturnToMainMenu_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (UP48LobbyTravelSubsystem* TravelSubsystem = GameInstance
		? GameInstance->GetSubsystem<UP48LobbyTravelSubsystem>() : nullptr)
	{
		TravelSubsystem->ClearLobbyReturnContext();
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
	ClientTravel(P48LobbyPlayerController::MainMenuMapPath, TRAVEL_Absolute);
}

void AP48LobbyPlayerController::SetPendingLobbyReturn(
	int32 RoomId, const FGuid& MemberId)
{
	if (!HasAuthority()) return;
	PendingReturnRoomId = RoomId;
	PendingReturnMemberId = MemberId;
}

bool AP48LobbyPlayerController::ConsumePendingLobbyReturn(
	int32& OutRoomId, FGuid& OutMemberId)
{
	if (!HasAuthority() || PendingReturnRoomId < 1
		|| !PendingReturnMemberId.IsValid()) return false;
	OutRoomId = PendingReturnRoomId;
	OutMemberId = PendingReturnMemberId;
	PendingReturnRoomId = INDEX_NONE;
	PendingReturnMemberId.Invalidate();
	return true;
}

void AP48LobbyPlayerController::RequestCreateLobby(
	bool bUsePassword, const FString& Password)
{
	if (IsLocalController())
	{
		Server_CreateLobby(bUsePassword, Password);
	}
}

void AP48LobbyPlayerController::RequestCreateLobbyWithSettings(
	const FString& RoomTitle, bool bUsePassword, const FString& Password)
{
	if (IsLocalController())
	{
		Server_CreateLobbyWithSettings(RoomTitle, bUsePassword, Password);
	}
}

void AP48LobbyPlayerController::Server_CreateLobby_Implementation(
	bool bUsePassword, const FString& Password)
{
	AP48LobbyGameMode* Mode = GetWorld()->GetAuthGameMode<AP48LobbyGameMode>();
	FText Error;
	if (!IsValid(Mode))
	{
		Client_ReportLobbyRequestFailure(FText::FromString(TEXT("Lobby is unavailable.")));
	}
	else if (!Mode->RequestCreateLobby(this, bUsePassword, Password, Error))
	{
		Client_ReportLobbyRequestFailure(Error);
	}
}

void AP48LobbyPlayerController::Server_CreateLobbyWithSettings_Implementation(
	const FString& RoomTitle, bool bUsePassword, const FString& Password)
{
	AP48LobbyGameMode* Mode = GetWorld()->GetAuthGameMode<AP48LobbyGameMode>();
	FText Error;
	if (!IsValid(Mode))
	{
		Client_ReportLobbyRequestFailure(FText::FromString(TEXT("Lobby is unavailable.")));
	}
	else if (!Mode->RequestCreateLobbyWithSettings(
		this, RoomTitle, bUsePassword, Password, Error))
	{
		Client_ReportLobbyRequestFailure(Error);
	}
}

void AP48LobbyPlayerController::RequestJoinLobby()
{
	if (IsLocalController())
	{
		Server_JoinLobby();
	}
}

void AP48LobbyPlayerController::RequestJoinLobbyById(
	int32 RoomId, const FString& Password)
{
	if (IsLocalController())
	{
		Server_JoinLobbyById(RoomId, Password);
	}
}

void AP48LobbyPlayerController::Server_JoinLobby_Implementation()
{
	AP48LobbyGameMode* Mode = GetWorld()->GetAuthGameMode<AP48LobbyGameMode>();
	FText Error;
	if (!IsValid(Mode))
	{
		Client_ReportLobbyRequestFailure(FText::FromString(TEXT("Lobby is unavailable.")));
	}
	else if (!Mode->RequestJoinLobby(this, Error))
	{
		Client_ReportLobbyRequestFailure(Error);
	}
}

void AP48LobbyPlayerController::Server_JoinLobbyById_Implementation(
	int32 RoomId, const FString& Password)
{
	AP48LobbyGameMode* Mode = GetWorld()->GetAuthGameMode<AP48LobbyGameMode>();
	FText Error;
	if (!IsValid(Mode))
	{
		Client_ReportLobbyRequestFailure(FText::FromString(TEXT("Lobby is unavailable.")));
	}
	else if (!Mode->RequestJoinLobbyById(this, RoomId, Password, Error))
	{
		Client_ReportLobbyRequestFailure(Error);
	}
}

void AP48LobbyPlayerController::RequestLeaveLobby()
{
	if (IsLocalController())
	{
		Server_LeaveLobby();
	}
}

void AP48LobbyPlayerController::Server_LeaveLobby_Implementation()
{
	if (AP48LobbyGameMode* Mode = GetWorld()->GetAuthGameMode<AP48LobbyGameMode>())
	{
		Mode->LeaveLobby(this);
	}
}

void AP48LobbyPlayerController::RequestReturnToMainMenu()
{
	if (IsLocalController())
	{
		Server_ReturnToMainMenu();
	}
}

void AP48LobbyPlayerController::Server_ReturnToMainMenu_Implementation()
{
	AP48LobbyGameMode* Mode = GetWorld()->GetAuthGameMode<AP48LobbyGameMode>();
	if (!IsValid(Mode))
	{
		Client_ReportLobbyRequestFailure(
			FText::FromString(TEXT("Lobby is unavailable.")));
		return;
	}

	// Remove the member immediately instead of waiting for the network logout,
	// then let the owning client disconnect and load its local main-menu map.
	Mode->LeaveLobby(this);
	Client_ReturnToMainMenu();
}

void AP48LobbyPlayerController::RequestLobbyReady(bool bNewReady)
{
	if (IsLocalController())
	{
		Server_SetLobbyReady(bNewReady);
	}
}

void AP48LobbyPlayerController::RequestStartGame()
{
	if (IsLocalController())
	{
		Server_RequestStartGame();
	}
}

bool AP48LobbyPlayerController::IsLobbyHost() const
{
	const AP48LobbyGameState* State = GetWorld()
		? GetWorld()->GetGameState<AP48LobbyGameState>() : nullptr;
	return IsInLobby() && IsValid(State) && State->IsHost(PlayerState);
}

bool AP48LobbyPlayerController::CanStartLobbyGame() const
{
	const AP48LobbyGameState* State = GetWorld()
		? GetWorld()->GetGameState<AP48LobbyGameState>() : nullptr;
	return IsLobbyHost() && IsValid(State) && State->CanPlayerStartGame(PlayerState);
}

bool AP48LobbyPlayerController::IsLocalPlayerLobbyReady() const
{
	const AP48PlayerState* State = GetPlayerState<AP48PlayerState>();
	return IsInLobby() && IsValid(State) && !IsLobbyHost() && State->IsReady();
}

bool AP48LobbyPlayerController::CanChangeLobbyReady() const
{
	return IsInLobby() && !IsLobbyHost() && !IsCurrentLobbyReturningToLobby();
}

bool AP48LobbyPlayerController::IsCurrentLobbyReturningToLobby() const
{
	const AP48LobbyGameState* State = GetWorld()
		? GetWorld()->GetGameState<AP48LobbyGameState>() : nullptr;
	return IsInLobby() && IsValid(State)
		&& State->IsRoomReturningToLobby(LobbyRoomId);
}

FText AP48LobbyPlayerController::GetCurrentLobbyStatusText() const
{
	const AP48LobbyGameState* State = GetWorld()
		? GetWorld()->GetGameState<AP48LobbyGameState>() : nullptr;
	return IsInLobby() && IsValid(State)
		? State->GetRoomStatusText(LobbyRoomId) : FText::GetEmpty();
}

FText AP48LobbyPlayerController::GetCurrentLobbyPlayerListText() const
{
	const AP48LobbyGameState* State = GetWorld()
		? GetWorld()->GetGameState<AP48LobbyGameState>() : nullptr;
	return IsValid(State)
		? State->GetLobbyPlayerListTextForRoom(LobbyRoomId)
		: FText::GetEmpty();
}

void AP48LobbyPlayerController::Server_SetLobbyReady_Implementation(bool bNewReady)
{
	AP48LobbyGameMode* Mode = GetWorld()->GetAuthGameMode<AP48LobbyGameMode>();
	if (!IsValid(Mode))
	{
		return;
	}
	Mode->RequestLobbyReady(this, bNewReady);
}

void AP48LobbyPlayerController::Server_RequestStartGame_Implementation()
{
	if (AP48LobbyGameMode* Mode = GetWorld()->GetAuthGameMode<AP48LobbyGameMode>())
	{
		Mode->RequestStartGame(this);
	}
}
