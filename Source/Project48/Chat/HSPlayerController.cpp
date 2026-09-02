#include "HSPlayerController.h"
#include "P48ChatWidget.h"
#include "P48ChatInput.h"
#include "Components/EditableTextBox.h"

//FInputModeUIOnly InputModeUIOnly; // Input Mode(입력 모드)를 설정하기 위한 구조체 (게임 플레이보다는 UI가 입력을 받도록 설정하는 모드)
//SetInputMode(InputModeUIOnly); // PlayerController의 입력 모드를 UI 전용으로 설정
// IsValid()는 해당 객체(Object)가 유효한지 확인하는 함수

void AHSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	FInputModeGameOnly InputModeGameOnly;
	SetInputMode(InputModeGameOnly);
	
	// 게임 중 계속 보이는 채팅 메시지 UI
	if (IsValid(ChatWidgetClass) == true)
	{
		ChatWidgetInstance = CreateWidget<UP48ChatWidget>(this, ChatWidgetClass);

		if (IsValid(ChatWidgetInstance) == true)
		{
			ChatWidgetInstance->AddToViewport();
		}
	}
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
		PrintChatMessageString(InChatMessageString);
	}

	CloseChatInput();
}

void AHSPlayerController::PrintChatMessageString(const FString& InChatMessageString)
{
	if (IsValid(ChatWidgetInstance) == false) return;
	
	ChatWidgetInstance->AddChatMessage(InChatMessageString);
}