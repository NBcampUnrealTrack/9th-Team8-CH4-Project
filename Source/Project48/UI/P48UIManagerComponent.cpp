#include "P48UIManagerComponent.h"
#include "Project48/UI/HS/HSGameStateBase.h"
#include "Project48/UI/Chat/P48ChatWidget.h"
#include "GameFramework/PlayerState.h"
#include "HS/HSPlayerController.h"
#include "Project48/UI/Chat/ChatMessageData.h"
#include "Project48/UI/P48HUD.h"

UP48UIManagerComponent::UP48UIManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UP48UIManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	
	AHSPlayerController* PC = Cast<AHSPlayerController>(GetOwner());
	if (IsValid(PC) == false) return;
	PC->SetInputMode(FInputModeGameOnly());
	
	// 게임 중 계속 보이는 채팅 메시지 UI
	if (IsValid(ChatWidgetClass) == false) return;

	ChatWidgetInstance = CreateWidget<UP48ChatWidget>(PC, ChatWidgetClass);
	HUDInstance = CreateWidget<UP48HUD>(PC, HUDClass);

	if (IsValid(ChatWidgetInstance) == false) return;
	if (IsValid(HUDInstance) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("HUD생성 못함"));
	}
	
	HUDInstance->AddToViewport();
}

void UP48UIManagerComponent::HUDOpenChatInput()
{
	if (IsValid(ChatWidgetInstance) == false) return;

	bIsChatInputOpen = true;
	AHSPlayerController* PC = Cast<AHSPlayerController>(GetOwner());
	if (IsValid(PC) == false) return;
	PC->SetInputMode(FInputModeGameAndUI());
	
	ChatWidgetInstance->OpenChatInput();
	
	PC->bShowMouseCursor = true;
}

void UP48UIManagerComponent::HUDCloseChatInput()
{
	if (IsValid(ChatWidgetInstance) == false) return;

	bIsChatInputOpen = false;

	ChatWidgetInstance->CloseChatInput();

	AHSPlayerController* PC = Cast<AHSPlayerController>(GetOwner());
	if (IsValid(PC) == false) return;
	PC->SetInputMode(FInputModeGameOnly());

	PC->bShowMouseCursor = false;
}

void UP48UIManagerComponent::SetChatMessageString(const FString& InChatMessageString)
{
	if (InChatMessageString.IsEmpty() == false)
	{
		ServerSendChatMessage(InChatMessageString);
	}

	HUDCloseChatInput();
}

void UP48UIManagerComponent::PrintChatMessageString(const FChatMessage& InChatMessage)
{
	if (IsValid(ChatWidgetInstance) == false) return;
	
	ChatWidgetInstance->AddChatMessage(InChatMessage);
}

void UP48UIManagerComponent::ServerSendChatMessage_Implementation(
	const FString& InChatMessage)
{   
	/* 추후에 GSB변경 */
	AHSGameStateBase* GameState = GetWorld()->GetGameState<AHSGameStateBase>();
	if (IsValid(GameState) == false) return;
	AHSPlayerController* PC = Cast<AHSPlayerController>(GetOwner());
	if (IsValid(PC) == false) return;
	FChatMessage ChatMessage;
	
	ChatMessage.PlayerName = PC->GetPlayerState<APlayerState>()->GetPlayerName();
	ChatMessage.Message = InChatMessage;
	
	GameState->MulticastReceiveChatMessage(ChatMessage);
}