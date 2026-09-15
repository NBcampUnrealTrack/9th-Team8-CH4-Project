#include "P48HUD.h"

#include "P48RankingWidget.h"
#include "Project48/UI/Chat/P48ChatInput.h"
#include "Project48/UI/Chat/P48ChatMessage.h"
#include "Components/EditableTextBox.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"

void UP48HUD::NativeConstruct()
{
	Super::NativeConstruct();

	ChatInput->SetVisibility(ESlateVisibility::Collapsed);
	RankingWidget->UpdateRanking();
}

void UP48HUD::AddChatMessage(const FChatMessage& InChatMessage)
{
	if (IsValid(VerticalBox_ChatMessages) == false) return;

	UP48ChatMessage* NewChatMessage = CreateWidget<UP48ChatMessage>(GetWorld(), ChatMessageClass);
	if (IsValid(NewChatMessage) == false) return;
	
	NewChatMessage->SetChatMessage(InChatMessage);

	// 최대 메시지 개수 이상이라면 가장 오래된 메시지 삭제
	if (VerticalBox_ChatMessages->GetChildrenCount() >= MaxChatMessageCount)
	{
		VerticalBox_ChatMessages->RemoveChildAt(0); // RemoveChildAt(0) 은 첫 번째 자식, 가장 오래된 채팅
	}
	
	VerticalBox_ChatMessages->AddChild(NewChatMessage); // 새로운 메시지 추가
	
	if (IsValid(ScrollBox_Chat) == false) return;
	ScrollBox_Chat->ScrollToEnd(); // 스크롤 맨 아래로
}

void UP48HUD::OpenChatInput()
{
	if (IsValid(ChatInput) == false) return;
	ChatInput->SetVisibility(ESlateVisibility::Visible);
	
	if (IsValid(ChatInput->EditableTextBox_ChatInput) == false) return;
	ChatInput->EditableTextBox_ChatInput->SetKeyboardFocus();
}

void UP48HUD::CloseChatInput()
{
	if (IsValid(ChatInput) == false)  return;
	ChatInput->SetVisibility(ESlateVisibility::Collapsed);
}