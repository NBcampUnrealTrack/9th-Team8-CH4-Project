#include "P48UIManagerComponent.h"
#include "Project48/UI/HS/HSGameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HS/HSPlayerController.h"
#include "Project48/Character/P48PlayerState.h"
#include "Project48/UI/Chat/ChatMessageData.h"
#include "Project48/UI/P48HUD.h"
#include "Project48/UI/MainMenu/P48MainMenuWidget.h"
#include "Project48/UI/MainMenu/P48InputNicknameWidget.h"

UP48UIManagerComponent::UP48UIManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UP48UIManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	
	UWorld* World = GetWorld();
	if (IsValid(World) == false) return;
	
	FString LevelName = World->GetMapName();

	if (LevelName.Contains(TEXT("HSLevel")))
	{
		ShowMainMenu();
	}
	else if (LevelName.Contains(TEXT("HSGameLevel")))
	{
		ShowHUD();
	}
	
	/*
	// 게임 중 계속 보이는 채팅 메시지 UI
	if (IsValid(HUDClass) == false) return;
	
	HUDInstance = CreateWidget<UP48HUD>(PC, HUDClass);
	
	if (IsValid(HUDInstance) == false)
	{
		UE_LOG(LogTemp, Error, TEXT("HUD생성 못함"));
		return;
	}
	
	HUDInstance->AddToViewport();
	 */
}

void UP48UIManagerComponent::ShowMainMenu()
{
	AHSPlayerController* PC = Cast<AHSPlayerController>(GetOwner());
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
	AHSPlayerController* PC = Cast<AHSPlayerController>(GetOwner());
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
	AHSPlayerController* PC = Cast<AHSPlayerController>(GetOwner());
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
	
	// TODO 로비 인스턴스
	/* 
	if (IsValid(LobbyInstance))
	{
		LobbyInstance->RemoveFromParent();
		LobbyInstance = nullptr;
	}
	 */
}

void UP48UIManagerComponent::Server_CheckNickname_Implementation(const FString& Nickname)
{
	// TODO 닉네임 중복 관련
	
	// AHSPlayerController* PC = Cast<AHSPlayerController>(GetOwner());
	// if (IsValid(PC) == false) return;
	// if (Nickname.IsEmpty()) return;
	//
	// for (APlayerState* PlayerState : GetWorld()->GetGameState()->PlayerArray)
	// {
	// 	AP48PlayerState* PS = Cast<AP48PlayerState>(PlayerState);
	//
	// 	if (IsValid(PS) == false) // 정보를 불러오지 못하거나 로컬 데이터가 없으면 스킵
	// 		continue;
	//
	// 	if (PS == PC->GetPlayerState<AP48PlayerState>()) // 이미 로컬 데이터가 있는 경우 스킵(내 닉네임)
	// 		continue;
	//
	// 	if (PS->GetPlayerName() == Nickname)
	// 	{
	// 		// 중복
	// 		Client_NicknameCheckResult(false);
	// 		return;
	// 	}
	// }
	//
	// // 중복이 아니면 내 PlayerState에 저장
	// AP48PlayerState* MyPS = PC->GetPlayerState<AP48PlayerState>();
	//
	// if (IsValid(MyPS) == false) return;
	// MyPS->SetNickname(Nickname);
	//
	Client_NicknameCheckResult(true);
}

void UP48UIManagerComponent::Client_NicknameCheckResult_Implementation(bool bSuccess)
{
	if (bSuccess)
	{
		// 닉네임 사용 가능
		// 레벨 이동 등
		AHSPlayerController* PC = Cast<AHSPlayerController>(GetOwner());
		if (IsValid(PC) == false) return;
		ShowHUD();
		PC->ClientTravel(TEXT("127.0.0.1:17777"), ETravelType::TRAVEL_Absolute);
		// TODO 로비로 이동
	}
	else
	{
		// 닉네임 중복
		// UI에 "이미 사용 중인 닉네임입니다." 출력
		if (IsValid(InputNicknameInstance) == false) return;
		InputNicknameInstance->SetErrorText(("This nickname is already in use"));
		
		
	}
}

void UP48UIManagerComponent::HUDOpenChatInput()
{
	if (IsValid(HUDInstance) == false) return;

	bIsChatInputOpen = true;
	AHSPlayerController* PC = Cast<AHSPlayerController>(GetOwner());
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
	if (IsValid(HUDInstance) == false) return;
	
	HUDInstance->AddChatMessage(InChatMessage);
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