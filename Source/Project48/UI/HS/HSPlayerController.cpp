#include "HSPlayerController.h"
#include "HSGameStateBase.h"
#include "Project48/UI/Chat/P48ChatWidget.h"
#include "GameFramework/PlayerState.h"
#include "Project48/UI/Chat/ChatMessageData.h"
#include "Project48/UI/P48HUD.h"

//FInputModeUIOnly InputModeUIOnly; // Input Mode(입력 모드)를 설정하기 위한 구조체 (게임 플레이보다는 UI가 입력을 받도록 설정하는 모드)
//SetInputMode(InputModeUIOnly); // PlayerController의 입력 모드를 UI 전용으로 설정
// IsValid()는 해당 객체(Object)가 유효한지 확인하는 함수

void AHSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	FInputModeGameOnly InputModeGameOnly;
	SetInputMode(InputModeGameOnly);
	
	// 게임 중 계속 보이는 채팅 메시지 UI
	if (IsValid(ChatWidgetClass) == false) return;

	ChatWidgetInstance = CreateWidget<UP48ChatWidget>(this, ChatWidgetClass);
	CombatWidgetInstance = CreateWidget<UP48HUD>(this, CombatWidgetClass);

	if (IsValid(ChatWidgetInstance) == false) return;
	if (IsValid(CombatWidgetInstance) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("콤뱃생성 못함"));
	}
	
	CombatWidgetInstance->AddToViewport();
}

void AHSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsValid(InputComponent))
	{
		InputComponent->BindKey(
			EKeys::Enter,
			IE_Pressed,
			this,
			&AHSPlayerController::ToggleChatInput
		);
	}
}

void AHSPlayerController::ToggleChatInput()
{
	if (bIsChatInputOpen == true) return;

	OpenChatInput();
}

void AHSPlayerController::OpenChatInput()
{
	if (IsValid(ChatWidgetInstance) == false) return;

	bIsChatInputOpen = true;

	FInputModeGameAndUI InputModeGameAndUI;
	SetInputMode(InputModeGameAndUI);
	
	ChatWidgetInstance->OpenChatInput();
	
	//bShowMouseCursor = true;
}

void AHSPlayerController::CloseChatInput()
{
	if (IsValid(ChatWidgetInstance) == false) return;

	bIsChatInputOpen = false;

	ChatWidgetInstance->CloseChatInput();

	FInputModeGameOnly InputModeGameOnly;
	SetInputMode(InputModeGameOnly);

	//bShowMouseCursor = false;
}

void AHSPlayerController::SetChatMessageString(const FString& InChatMessageString)
{
	if (InChatMessageString.IsEmpty() == false)
	{
		ServerSendChatMessage(InChatMessageString);
	}

	CloseChatInput();
}

void AHSPlayerController::PrintChatMessageString(const FChatMessage& InChatMessage)
{
	if (IsValid(ChatWidgetInstance) == false) return;
	
	ChatWidgetInstance->AddChatMessage(InChatMessage);
}

void AHSPlayerController::ServerSendChatMessage_Implementation(
	const FString& InChatMessage)
{   
	/* 추후에 GSB변경 */
	AHSGameStateBase* GameState = GetWorld()->GetGameState<AHSGameStateBase>();

	if (IsValid(GameState) == false) return;
	
	FChatMessage ChatMessage;
	
	ChatMessage.PlayerName = GetPlayerState<APlayerState>()->GetPlayerName();
	ChatMessage.Message = InChatMessage;
	
	GameState->MulticastReceiveChatMessage(ChatMessage);
}