#include "HSPlayerController.h"
#include "Project48/UI/P48UIManagerComponent.h"

//FInputModeUIOnly InputModeUIOnly; // Input Mode(입력 모드)를 설정하기 위한 구조체 (게임 플레이보다는 UI가 입력을 받도록 설정하는 모드)
//SetInputMode(InputModeUIOnly); // PlayerController의 입력 모드를 UI 전용으로 설정
// IsValid()는 해당 객체(Object)가 유효한지 확인하는 함수

AHSPlayerController::AHSPlayerController()
{
	UIManagerComp = CreateDefaultSubobject<UP48UIManagerComponent>(TEXT("UIManagerComponent"));
}

void AHSPlayerController::BeginPlay()
{
	Super::BeginPlay();
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
	UP48UIManagerComponent* UIManager = FindComponentByClass<UP48UIManagerComponent>();
	if (IsValid(UIManager) == false) return;
	
	if (UIManager->GetIsChatInputOpen() == true) return;

	UIManager->HUDOpenChatInput();
}