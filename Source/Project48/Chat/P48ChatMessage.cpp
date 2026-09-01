#include "P48ChatMessage.h"
#include "Components/TextBlock.h"

void UP48ChatMessage::SetChatMessage(const FString& InChatMessage)
{
	TextBlock_ChatMessage->SetText(
		FText::FromString(InChatMessage)
	);
}