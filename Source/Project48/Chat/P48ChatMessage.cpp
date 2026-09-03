#include "P48ChatMessage.h"
#include "Components/TextBlock.h"
#include "ChatMessageData.h"

void UP48ChatMessage::SetChatMessage(const FChatMessage& InChatMessage)
{
	const FString ChatText = FString::Printf(TEXT("%s : %s"), *InChatMessage.PlayerName, *InChatMessage.Message);

	TextBlock_ChatMessage->SetText(FText::FromString(ChatText));
}