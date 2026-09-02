#include "HSGameStateBase.h"
#include "HSPlayerController.h"

void AHSGameStateBase::MulticastReceiveChatMessage_Implementation(const FString& InChatMessage)
{
	AHSPlayerController* PC = Cast<AHSPlayerController>(GetWorld()->GetFirstPlayerController());

	if (IsValid(PC) == true)
	{
		PC->PrintChatMessageString(InChatMessage);
	}
}
