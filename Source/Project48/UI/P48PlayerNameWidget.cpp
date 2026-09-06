#include "P48PlayerNameWidget.h"
#include "Components/TextBlock.h"

void UP48PlayerNameWidget::SetPlayerName(const FString& InPlayerName)
{
	if (IsValid(TextBlock_PlayerNameText) == false) return;
	
	TextBlock_PlayerNameText->SetText(FText::FromString(InPlayerName));
}