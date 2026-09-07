#include "P48UIManagerComponent.h"
#include "Project48/UI/HS/HSGameStateBase.h"
#include "Project48/UI/Chat/P48ChatWidget.h"
#include "GameFramework/PlayerState.h"
#include "HS/HSPlayerController.h"
#include "Project48/Character/P48PlayerCharacter.h"
#include "Project48/UI/Chat/ChatMessageData.h"
#include "Project48/UI/P48HUD.h"

UP48UIManagerComponent::UP48UIManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UP48UIManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	
	AHSPlayerController* PC = UIManagerGetController();
	PC->SetInputMode(FInputModeGameOnly());
	
	// 게임 중 계속 보이는 채팅 메시지 UI
	if (IsValid(ChatWidgetClass) == false) return;

	ChatWidgetInstance = CreateWidget<UP48ChatWidget>(this, ChatWidgetClass);
	HUDInstance = CreateWidget<UP48HUD>(this, HUDClass);

	if (IsValid(ChatWidgetInstance) == false) return;
	if (IsValid(HUDInstance) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("HUD생성 못함"));
	}
	
	HUDInstance->AddToViewport();
}

AHSPlayerController* UP48UIManagerComponent::UIManagerGetController()
{
	AP48PlayerCharacter* P48Character = Cast<AP48PlayerCharacter>(GetOwner());
	if (P48Character == nullptr) return nullptr;
	AHSPlayerController* PC = Cast<AHSPlayerController>(P48Character->GetController());
	if (PC == nullptr) return nullptr;
	else
	{
		return PC;
	}
}

void UP48UIManagerComponent::OpenChatInput()
{
	if (IsValid(ChatWidgetInstance) == false) return;

	bIsChatInputOpen = true;
	AHSPlayerController* PC = UIManagerGetController();
	PC->SetInputMode(FInputModeGameAndUI());
	
	ChatWidgetInstance->OpenChatInput();
	
	PC->bShowMouseCursor = true;
}

void UP48UIManagerComponent::CloseChatInput()
{
	if (IsValid(ChatWidgetInstance) == false) return;

	bIsChatInputOpen = false;

	ChatWidgetInstance->CloseChatInput();

	AHSPlayerController* PC = UIManagerGetController();
	PC->SetInputMode(FInputModeGameOnly());

	PC->bShowMouseCursor = false;
}

void UP48UIManagerComponent::SetChatMessageString(const FString& InChatMessageString)
{
	if (InChatMessageString.IsEmpty() == false)
	{
		ServerSendChatMessage(InChatMessageString);
	}

	CloseChatInput();
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
	AHSPlayerController* PC = UIManagerGetController();
	FChatMessage ChatMessage;
	
	ChatMessage.PlayerName = PC->GetPlayerState<APlayerState>()->GetPlayerName();
	ChatMessage.Message = InChatMessage;
	
	GameState->MulticastReceiveChatMessage(ChatMessage);
}