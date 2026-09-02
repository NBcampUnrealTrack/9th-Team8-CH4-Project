#include "HSGameStateBase.h"
#include "HSPlayerController.h"

void AHSGameStateBase::MulticastReceiveChatMessage_Implementation(const FChatMessage& InChatMessage)
{
	/* 추후에 PC변경 */
	AHSPlayerController* PC = Cast<AHSPlayerController>(GetWorld()->GetFirstPlayerController());

	if (IsValid(PC) == true)
	{
		PC->PrintChatMessageString(InChatMessage);
	}
}
