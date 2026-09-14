#include "P48UIManagerComponent.h"
#include "Project48/UI/HS/HSGameStateBase.h"
#include "GameFramework/PlayerState.h"
//#include "HS/HSPlayerController.h"
//#include "HS/HSPlayerState.h"
#include "Project48/Character/P48PlayerState.h"
#include "Project48/Database/P48NicknameDatabaseSubsystem.h"
#include "Project48/UI/Chat/ChatMessageData.h"
#include "Project48/UI/P48HUD.h"
#include "Project48/UI/MainMenu/P48MainMenuWidget.h"
#include "Project48/UI/MainMenu/P48InputNicknameWidget.h"
#include "HAL/PlatformProcess.h"
#include "Project48/Character/P48PlayerController.h"
#include "Project48/Game/P48GameInstance.h"

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
		UE_LOG(LogTemp, Log, TEXT("들어옴"));
		ShowHUD();
		UP48GameInstance* P48GameInstance = Cast<UP48GameInstance>(PC->GetGameInstance());
		
		if (P48GameInstance)
		{
			AP48PlayerState* PS = PC->GetPlayerState<AP48PlayerState>();
			if (PS)
			{
				PS->SetNickname(P48GameInstance->GetNickname());
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
}

/*void UP48UIManagerComponent::ServerRegisterNickname_Implementation(const FString& Nickname)
{
	
	AHSPlayerController* HSPC = Cast<AHSPlayerController>(GetOwner());
	if (IsValid(HSPC) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("HSPC 없습니다."));
		return;
	}
	
	if (HSPC->HasAuthority() == false)
	{
		UE_LOG(LogTemp, Error, TEXT("false 입니다."));
		return;
	}
	
	UP48NicknameDatabaseSubsystem* DatabaseSubsystem = HSPC->GetGameInstance()->GetSubsystem<UP48NicknameDatabaseSubsystem>();
	UE_LOG(
	LogTemp,
	Warning,
	TEXT(
		"[Nickname RPC] PID=%u, HSPC=%p, GameInstance=%p, DBSubsystem=%p"
	),
	FPlatformProcess::GetCurrentProcessId(),
	HSPC,
	HSPC->GetGameInstance(),
	DatabaseSubsystem
);
	if (IsValid(DatabaseSubsystem) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("Nickname DB Subsystem이 없습니다."));
		return;
	}

	AHSPlayerState* HSPS = HSPC->GetPlayerState<AHSPlayerState>();
	if (IsValid(HSPS) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("HSPS가 없습니다."));
		return;
	}
	
	FString UserID;
	if (HSPS->GetUniqueId().IsValid() == true)
	{
		UserID = HSPS->GetUniqueId()->ToString();
	}
	else
	{
		UserID = HSPC->GetName();
	}
	
	UE_LOG(LogTemp, Error, TEXT("[Nickname] 서버 닉네임 등록 요청 - UserID: [%s], Nickname: [%s]"), *UserID, *Nickname);
	
	const EP48NicknameRegistrationResult Result = DatabaseSubsystem->TryRegisterNickname(UserID, Nickname);

	if (Result != EP48NicknameRegistrationResult::Success)
	{
		// 닉네임 등록 실패
		UE_LOG(LogTemp, Error, TEXT("[Nickname] 닉네임 등록 실패 - Result: %d"), static_cast<int32>(Result));
		ClientNicknameRegistrationFailed(Result);
		return;
	}

	// DB 저장 성공
	const FString DisplayNickname = Nickname.TrimStartAndEnd();

	HSPS->SetNickname(DisplayNickname);
	UE_LOG(LogTemp, Error, TEXT("[Nickname] 닉네임 등록 성공 - [%s]"), *DisplayNickname);
	ClientNicknameRegistrationSucceeded(DisplayNickname);
}

void UP48UIManagerComponent::ClientNicknameRegistrationSucceeded_Implementation(const FString& Nickname)
{
	UE_LOG(LogTemp, Log, TEXT("닉네임 등록 성공: %s"), *Nickname);
	
	AHSPlayerController* PC = Cast<AHSPlayerController>(GetOwner());
	if (IsValid(PC) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("[Nickname] PC가 없습니다."));
	}
	//ShowHUD();
	PC->ClientTravel(TEXT("127.0.0.1:17777"), ETravelType::TRAVEL_Absolute);
	
}

void UP48UIManagerComponent::ClientNicknameRegistrationFailed_Implementation(EP48NicknameRegistrationResult Result)
{
	if (IsValid(InputNicknameInstance) == false)
	{
		UE_LOG(LogTemp, Log, TEXT("InputNicknameInstance이 없는데용? 띠용?"));
		return;
	}
	switch (Result)
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
		// "이미 닉네임이 등록되어 있습니다."
		InputNicknameInstance->SetErrorText(("Nickname is already registered."));
		break;

	default:
		// "닉네임 등록에 실패했습니다."
		InputNicknameInstance->SetErrorText(("Failed to register the nickname."));
		break;
	}
}*/

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