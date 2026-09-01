#include "HSPlayerController.h"
#include "P48ChatInput.h"
#include "P48ChatWidget.h"
#include "P48ChatMessage.h"
#include "Kismet/KismetSystemLibrary.h"

void AHSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	FInputModeUIOnly InputModeUIOnly; // Input Mode(입력 모드)를 설정하기 위한 구조체 (게임 플레이보다는 UI가 입력을 받도록 설정하는 모드)
	SetInputMode(InputModeUIOnly); // PlayerController의 입력 모드를 UI 전용으로 설정

	// IsValid()는 해당 객체(Object)가 유효한지 확인하는 함수
	if (IsValid(ChatWidgetClass) == true)
	{
		ChatWidgetInstance = CreateWidget<UP48ChatWidget>(this, ChatWidgetClass);

		if (IsValid(ChatWidgetInstance) == true)
		{
			ChatWidgetInstance->AddToViewport();
		}
	}
}

void AHSPlayerController::SetChatMessageString(const FString& InChatMessageString)
{
	PrintChatMessageString(InChatMessageString);
}

void AHSPlayerController::PrintChatMessageString(const FString& InChatMessageString)
{
	if (IsValid(ChatWidgetInstance) == false) return;
	
	ChatWidgetInstance->AddChatMessage(InChatMessageString);
}