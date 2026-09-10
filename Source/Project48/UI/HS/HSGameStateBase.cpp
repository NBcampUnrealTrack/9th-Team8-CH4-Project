#include "HSGameStateBase.h"
#include "HSPlayerController.h"
#include "Project48/UI/P48UIManagerComponent.h"

void AHSGameStateBase::MulticastReceiveChatMessage_Implementation(const FChatMessage& InChatMessage)
{
	/* 추후에 PC변경 */
	AHSPlayerController* PC = Cast<AHSPlayerController>(GetWorld()->GetFirstPlayerController());
	if (IsValid(PC) == false) return;
	UP48UIManagerComponent* UIManager = PC->FindComponentByClass<UP48UIManagerComponent>();
	if (IsValid(UIManager) == false) return;
	
	UIManager->PrintChatMessageString(InChatMessage);
}
