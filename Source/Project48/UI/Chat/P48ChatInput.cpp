#include "P48ChatInput.h"
#include "Components/EditableTextBox.h"
#include "Project48/Character/P48PlayerController.h"
#include "Project48/UI/P48UIManagerComponent.h"
//#include "Project48/UI/HS/HSPlayerController.h"

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
	
	APlayerController* OwningPC = GetOwningPlayer();
	if (IsValid(OwningPC) == false) return;
	
	/* 추후에 PC변경 */
	AP48PlayerController* PC = Cast<AP48PlayerController>(OwningPC);
	if (IsValid(PC) == false) return;
	UP48UIManagerComponent* UIManager = PC->FindComponentByClass<UP48UIManagerComponent>();
	
	UIManager->SetChatMessageString(Text.ToString());

	if (IsValid(EditableTextBox_ChatInput) == true)
	{
		EditableTextBox_ChatInput->SetText(FText());	
	}
}