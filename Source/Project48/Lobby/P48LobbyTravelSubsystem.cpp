#include "P48LobbyTravelSubsystem.h"

#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

namespace P48GameServerRetry
{
	constexpr double RetryIntervalSeconds = 3.0;
	constexpr double RetryLifetimeSeconds = 600.0;
}

void UP48LobbyTravelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(
			this, &ThisClass::HandleNetworkFailure);
	}
	RetryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickGameServerRetry), 0.25f);
}

void UP48LobbyTravelSubsystem::Deinitialize()
{
	if (GEngine && NetworkFailureHandle.IsValid())
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}
	NetworkFailureHandle.Reset();
	if (RetryTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RetryTickerHandle);
	}
	RetryTickerHandle.Reset();
	ClearGameServerRetry();
	Super::Deinitialize();
}

bool UP48LobbyTravelSubsystem::StoreLobbyReturnContext(
	const FString& Address, int32 RoomId, const FGuid& MemberId)
{
	const FString TrimmedAddress = Address.TrimStartAndEnd();
	FString Host;
	FString PortText;
	int32 Port = 0;
	const bool bValidAddress = TrimmedAddress.Split(TEXT(":"), &Host, &PortText,
		ESearchCase::CaseSensitive, ESearchDir::FromEnd)
		&& !Host.IsEmpty() && PortText.IsNumeric()
		&& LexTryParseString(Port, *PortText) && Port >= 1 && Port <= 65535
		&& !TrimmedAddress.Contains(TEXT("?"))
		&& !TrimmedAddress.Contains(TEXT("/"))
		&& !TrimmedAddress.Contains(TEXT("\\"))
		&& !TrimmedAddress.Contains(TEXT(" "));
	if (!bValidAddress || RoomId < 1 || !MemberId.IsValid()) return false;

	LobbyReturnAddress = TrimmedAddress;
	LobbyReturnRoomId = RoomId;
	LobbyReturnMemberId = MemberId;
	bReturnInProgress = false;
	return true;
}

bool UP48LobbyTravelSubsystem::BeginGameServerTravel(const FString& Destination)
{
	if (Destination.TrimStartAndEnd().IsEmpty()) return false;

	PendingGameServerDestination = Destination;
	bGameServerTravelPending = true;
	bGameServerConnectionAttemptInProgress = false;
	GameServerRetryAttempt = 0;
	const double Now = FPlatformTime::Seconds();
	NextGameServerRetryTime = Now;
	GameServerRetryDeadline = Now + P48GameServerRetry::RetryLifetimeSeconds;
	return TryGameServerTravel();
}

void UP48LobbyTravelSubsystem::ConfirmGameServerArrival()
{
	if (!bGameServerTravelPending) return;
	UE_LOG(LogTemp, Display, TEXT("[GameTravel] Game server arrival confirmed after %d attempt(s)."),
		GameServerRetryAttempt);
	ClearGameServerRetry();
}

void UP48LobbyTravelSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
	const ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (!bGameServerTravelPending) return;
	if (FailureType != ENetworkFailure::ConnectionLost
		&& FailureType != ENetworkFailure::ConnectionTimeout
		&& FailureType != ENetworkFailure::FailureReceived
		&& FailureType != ENetworkFailure::PendingConnectionFailure
		&& FailureType != ENetworkFailure::NetDriverCreateFailure)
	{
		return;
	}

	bGameServerConnectionAttemptInProgress = false;
	NextGameServerRetryTime = FPlatformTime::Seconds()
		+ P48GameServerRetry::RetryIntervalSeconds;
	UE_LOG(LogTemp, Warning,
		TEXT("[GameTravel] Connection failed (%s): %s. Retrying in %.0f seconds."),
		ENetworkFailure::ToString(FailureType), *ErrorString,
		P48GameServerRetry::RetryIntervalSeconds);
}

bool UP48LobbyTravelSubsystem::TickGameServerRetry(float DeltaTime)
{
	if (!bGameServerTravelPending || bGameServerConnectionAttemptInProgress) return true;

	const double Now = FPlatformTime::Seconds();
	if (Now >= GameServerRetryDeadline)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameTravel] Retry deadline expired; returning to the lobby."));
		ClearGameServerRetry();
		ReturnToLobby();
		return true;
	}
	if (Now >= NextGameServerRetryTime)
	{
		TryGameServerTravel();
	}
	return true;
}

bool UP48LobbyTravelSubsystem::TryGameServerTravel()
{
	if (!bGameServerTravelPending || PendingGameServerDestination.IsEmpty()) return false;
	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		NextGameServerRetryTime = FPlatformTime::Seconds()
			+ P48GameServerRetry::RetryIntervalSeconds;
		return false;
	}

	bGameServerConnectionAttemptInProgress = true;
	++GameServerRetryAttempt;
	PlayerController->SetInputMode(FInputModeGameOnly());
	PlayerController->bShowMouseCursor = false;
	UE_LOG(LogTemp, Display, TEXT("[GameTravel] Connecting to game server. Attempt=%d"),
		GameServerRetryAttempt);
	PlayerController->ClientTravel(PendingGameServerDestination, TRAVEL_Absolute);
	return true;
}

void UP48LobbyTravelSubsystem::ClearGameServerRetry()
{
	PendingGameServerDestination.Reset();
	bGameServerTravelPending = false;
	bGameServerConnectionAttemptInProgress = false;
	GameServerRetryAttempt = 0;
	NextGameServerRetryTime = 0.0;
	GameServerRetryDeadline = 0.0;
}

bool UP48LobbyTravelSubsystem::HasLobbyReturnContext() const
{
	return !LobbyReturnAddress.IsEmpty() && LobbyReturnRoomId >= 1
		&& LobbyReturnMemberId.IsValid();
}

bool UP48LobbyTravelSubsystem::ReturnToLobby()
{
	if (!HasLobbyReturnContext() || bReturnInProgress) return false;
	UWorld* World = GetWorld();
	APlayerController* PlayerController = World
		? World->GetFirstPlayerController() : nullptr;
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController()) return false;

	const FString Destination = FString::Printf(
		TEXT("%s?LobbyRoomId=%d?LobbyMemberId=%s"),
		*LobbyReturnAddress, LobbyReturnRoomId,
		*LobbyReturnMemberId.ToString(EGuidFormats::Digits));
	bReturnInProgress = true;
	PlayerController->SetInputMode(FInputModeGameOnly());
	PlayerController->bShowMouseCursor = false;
	PlayerController->ClientTravel(Destination, TRAVEL_Absolute);
	return true;
}

void UP48LobbyTravelSubsystem::ClearLobbyReturnContext()
{
	ClearGameServerRetry();
	LobbyReturnAddress.Reset();
	LobbyReturnRoomId = INDEX_NONE;
	LobbyReturnMemberId.Invalidate();
	bReturnInProgress = false;
}
