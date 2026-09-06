#include "P48ChatInput.h"
#include "Components/EditableTextBox.h"
#include "Project48/UI/HS/HSPlayerController.h"

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
	if (CommitMethod != ETextCommit::OnEnter) return;
	
	APlayerController* OwningPlayerController = GetOwningPlayer();
	if (IsValid(OwningPlayerController) == false) return;
	
	/* 추후에 PC변경 */
	AHSPlayerController* OwningHSPlayerController = Cast<AHSPlayerController>(OwningPlayerController);
	if (IsValid(OwningHSPlayerController) == false) return;
	
	OwningHSPlayerController->SetChatMessageString(Text.ToString());

	if (IsValid(EditableTextBox_ChatInput) == true)
	{
		EditableTextBox_ChatInput->SetText(FText());	
	}
}