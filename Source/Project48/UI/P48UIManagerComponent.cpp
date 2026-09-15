#include "P48UIManagerComponent.h"

#include "P48ResultUIWidget.h"
//#include "Project48/UI/HS/HSGameStateBase.h"
#include "Project48/Game/P48GameStateBase.h"
#include "Project48/Character/P48PlayerState.h"
#include "Project48/Database/P48NicknameDatabaseSubsystem.h"
#include "Project48/UI/Chat/ChatMessageData.h"
#include "Project48/UI/P48HUD.h"
#include "Project48/UI/MainMenu/P48MainMenuWidget.h"
#include "Project48/UI/MainMenu/P48InputNicknameWidget.h"
#include "HAL/PlatformProcess.h"
#include "Project48/Character/P48PlayerController.h"

UP48UIManagerComponent::UP48UIManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UP48UIManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	
	AP48PlayerController* PC = Cast<AP48PlayerController>(GetOwner());
	if (IsValid(PC) == false)
	{
		return;
	}
	if (PC->IsLocalController() == false)
	{
		return;
	}
	
	UWorld* World = GetWorld();
	if (IsValid(World) == false) return;
	
	FString LevelName = World->GetMapName();

	if (LevelName.Contains(TEXT("HSLevel")))
	{
		ShowMainMenu();
	}
	else if (LevelName.Contains(TEXT("P48_FlyingIslandMap")))
	{
		UE_LOG(LogTemp, Log, TEXT("P48_FlyingIslandMap 들어옴"));
		ShowHUD();
		
		AP48GameStateBase* GS = World->GetGameState<AP48GameStateBase>();

		if (IsValid(GS) == true)
		{
			GS->OnMatchEnded.AddDynamic(
				this,
				&UP48UIManagerComponent::HandleMatchEnded);

			// UIManager가 생성되기 전에 MatchEnd가
			// 이미 발생했을 가능성도 처리
			if (GS->MatchPhase == EP48MatchPhase::MatchEnd)
			{
				HandleMatchEnded();
			}
		}
	}
}

void UP48UIManagerComponent::ShowMainMenu()
{
	AP48PlayerController* PC = Cast<AP48PlayerController>(GetOwner());
	if (IsValid(PC) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("갑자기 왜그래"));
		return;
	}

	if (IsValid(MainMenuClass) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("MainMenuClass가 설정되지 않았습니다."));
		return;
	}

	// 기존 UI 제거
	ClearUI();

	MainMenuInstance = CreateWidget<UP48MainMenuWidget>(PC, MainMenuClass);
	if (IsValid(MainMenuInstance) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("MainMenu 생성 실패"));
		return;
	}

	MainMenuInstance->AddToViewport();

	PC->SetInputMode(FInputModeUIOnly());
	PC->bShowMouseCursor = true;
}

void UP48UIManagerComponent::ShowNickname()
{
	AP48PlayerController* PC = Cast<AP48PlayerController>(GetOwner());
	if (IsValid(PC) == false) return;

	if (IsValid(InputNicknameClass) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("NicknameClass가 설정되지 않았습니다."));
		return;
	}

	ClearUI();

	InputNicknameInstance = CreateWidget<UP48InputNicknameWidget>(PC, InputNicknameClass);
	if (IsValid(InputNicknameInstance) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("NicknameWidget 생성 실패"));
		return;
	}

	InputNicknameInstance->AddToViewport();

	PC->SetInputMode(FInputModeUIOnly());
	PC->bShowMouseCursor = true;
}

void UP48UIManagerComponent::ShowHUD()
{
	AP48PlayerController* PC = Cast<AP48PlayerController>(GetOwner());
	if (IsValid(PC) == false) return;
	
	if (IsValid(HUDClass) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("HUDClass 설정되지 않았습니다."));
		return;
	}

	ClearUI();
	
	HUDInstance = CreateWidget<UP48HUD>(PC, HUDClass);
	if (IsValid(HUDInstance) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("HUDInstance 생성 실패"));
		return;
	}
	HUDInstance->AddToViewport();
	PC->SetInputMode(FInputModeGameOnly());
	PC->bShowMouseCursor = false;
}

void UP48UIManagerComponent::ClearUI()
{
	if (IsValid(MainMenuInstance))
	{
		MainMenuInstance->RemoveFromParent();
		MainMenuInstance = nullptr;
	}

	if (IsValid(InputNicknameInstance))
	{
		InputNicknameInstance->RemoveFromParent();
		InputNicknameInstance = nullptr;
	}

	if (IsValid(HUDInstance))
	{
		HUDInstance->RemoveFromParent();
		HUDInstance = nullptr;
	}
	if (IsValid(ResultUIWidgetInstance))
	{
		ResultUIWidgetInstance->RemoveFromParent();
		ResultUIWidgetInstance = nullptr;
	}
}

void UP48UIManagerComponent::HUDOpenChatInput()
{
	if (IsValid(HUDInstance) == false) return;

	bIsChatInputOpen = true;
	AP48PlayerController* PC = Cast<AP48PlayerController>(GetOwner());
	if (IsValid(PC) == false) return;
	PC->SetInputMode(FInputModeGameAndUI());
	
	HUDInstance->OpenChatInput();
	
	PC->bShowMouseCursor = true;
}

void UP48UIManagerComponent::HUDCloseChatInput()
{
	if (IsValid(HUDInstance) == false) return;

	bIsChatInputOpen = false;

	HUDInstance->CloseChatInput();

	AP48PlayerController* PC = Cast<AP48PlayerController>(GetOwner());
	if (IsValid(PC) == false) return;
	PC->SetInputMode(FInputModeGameOnly());

	PC->bShowMouseCursor = false;
}

void UP48UIManagerComponent::ShowConnectionRejectedMessage(const EP48NicknameRegistrationResult& result)
{
	if (IsValid(InputNicknameInstance) == false)
	{
		UE_LOG(LogTemp, Log, TEXT("InputNicknameInstance이 없는데용? 띠용?"));
		return;
	}
	
	switch (result)
	{
	case EP48NicknameRegistrationResult::DuplicateNickname:
		// "이미 사용 중인 닉네임입니다."
		InputNicknameInstance->SetErrorText(("This nickname is already in use."));
		break;

	case EP48NicknameRegistrationResult::InvalidFormat:
		// "닉네임 형식이 잘못되었습니다." 
		InputNicknameInstance->SetErrorText(("The nickname format is invalid."));
		break;

	case EP48NicknameRegistrationResult::UserAlreadyRegistered:
		// "이미 UserID가 등록되어 있습니다."
		InputNicknameInstance->SetErrorText(("UserID is already registered."));
		break;

	default:
		// "닉네임 등록에 실패했습니다."
		InputNicknameInstance->SetErrorText(("Failed to register the nickname."));
		break;
	}
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
	if (IsValid(HUDInstance) == false) return;
	
	HUDInstance->AddChatMessage(InChatMessage);
}

void UP48UIManagerComponent::ServerSendChatMessage_Implementation(
	const FString& InChatMessage)
{   
	/* 추후에 GSB변경 */
	AP48GameStateBase* GameState = GetWorld()->GetGameState<AP48GameStateBase>();
	if (IsValid(GameState) == false) return;
	AP48PlayerController* PC = Cast<AP48PlayerController>(GetOwner());
	if (IsValid(PC) == false) return;
	AP48PlayerState* P48PS = PC->GetPlayerState<AP48PlayerState>();
	if (IsValid(P48PS) == false) return;
	
	FChatMessage ChatMessage;
	
	ChatMessage.PlayerName = P48PS->GetNickname();
	ChatMessage.Message = InChatMessage;
	
	GameState->MulticastReceiveChatMessage(ChatMessage);
}

void UP48UIManagerComponent::HandleMatchEnded()
{
	UE_LOG(LogTemp, Warning, TEXT("[UIManager] MatchEnd detected."));
    
	ShowResultUI();
}

void UP48UIManagerComponent::ShowResultUI()
{
	AP48PlayerController* PC = Cast<AP48PlayerController>(GetOwner());
	if (IsValid(PC) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("[UIManager] PlayerController is invalid."));
		return;
	}

	if (IsValid(ResultUIWidgetClass) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("[UIManager] ResultUIWidgetClass is not set."));
		return;
	}

	// 기존 HUD 제거
	ClearUI();

	// 이미 결과창이 있다면 다시 만들지 않는다.
	if (IsValid(ResultUIWidgetInstance) == true)
	{
		return;
	}

	ResultUIWidgetInstance = CreateWidget<UP48ResultUIWidget>(PC, ResultUIWidgetClass);
	if (IsValid(ResultUIWidgetInstance) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("[UIManager] ResultUIWidget creation failed."));
		return;
	}

	ResultUIWidgetInstance->AddToViewport();

	PC->SetInputMode(FInputModeUIOnly());
	PC->bShowMouseCursor = true;

	// PlayerArray에서 최종 결과 생성
	ResultUIWidgetInstance->BuildAndRefreshRanking();

	UE_LOG(LogTemp, Warning, TEXT("[UIManager] Result UI shown."));
}