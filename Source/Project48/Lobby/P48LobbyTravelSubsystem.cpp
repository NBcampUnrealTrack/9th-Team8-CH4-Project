#include "P48LobbyTravelSubsystem.h"

#include "GameFramework/PlayerController.h"

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
	LobbyReturnAddress.Reset();
	LobbyReturnRoomId = INDEX_NONE;
	LobbyReturnMemberId.Invalidate();
	bReturnInProgress = false;
}
