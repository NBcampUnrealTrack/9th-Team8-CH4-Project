#include "P48ChatInput.h"
#include "Components/EditableTextBox.h"
#include "HSPlayerController.h"

void UP48ChatInput::NativeConstruct()
{
	Super::NativeConstruct();

	if (EditableTextBox_ChatInput->OnTextCommitted.IsAlreadyBound(this, &ThisClass::OnChatInputTextCommitted) == false)
	{
		EditableTextBox_ChatInput->OnTextCommitted.AddDynamic(this, &ThisClass::OnChatInputTextCommitted);		
	}	
}

void UP48ChatInput::NativeDestruct()
{
	Super::NativeDestruct();

	if (EditableTextBox_ChatInput->OnTextCommitted.IsAlreadyBound(this, &ThisClass::OnChatInputTextCommitted) == true)
	{
		EditableTextBox_ChatInput->OnTextCommitted.RemoveDynamic(this, &ThisClass::OnChatInputTextCommitted);
	}
}

void UP48ChatInput::OnChatInputTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		if (Text.IsEmpty()) // 빈 메시지 막기
		{
			return;
		}
		
		APlayerController* OwningPlayerController = GetOwningPlayer();
		if (IsValid(OwningPlayerController) == true)
		{
			AHSPlayerController* OwningHSPlayerController = Cast<AHSPlayerController>(OwningPlayerController);
			if (IsValid(OwningHSPlayerController) == true)
			{
				OwningHSPlayerController->SetChatMessageString(Text.ToString());

				if (IsValid(EditableTextBox_ChatInput) == true)
				{
					EditableTextBox_ChatInput->SetText(FText());	
				}
			}
		}
	}
}