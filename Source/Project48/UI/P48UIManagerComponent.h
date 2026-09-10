#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "P48UIManagerComponent.generated.h"

struct FChatMessage;
class UP48ChatInput;
class UP48ChatWidget;
class UP48HUD;
class AHSPlayerController;
class UP48MainMenuWidget;
class UP48InputNicknameWidget;

UCLASS( Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECT48_API UP48UIManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UP48UIManagerComponent();
	
	virtual void BeginPlay() override;

protected:
	// 메인 메뉴
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UP48MainMenuWidget> MainMenuClass;
	UPROPERTY()
	TObjectPtr<UP48MainMenuWidget> MainMenuInstance;
	
	// 닉네임 입력
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UP48InputNicknameWidget> InputNicknameClass;
	UPROPERTY()
	TObjectPtr<UP48InputNicknameWidget> InputNicknameInstance;
	
	// HUD
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UP48HUD> HUDClass; // UP48HUD 기반으로 만들어진 WidgetBlueprint의 클래스
	UPROPERTY()
	TObjectPtr<UP48HUD> HUDInstance; // 실제로 생성된 채팅 위젯 객체를 저장하는 변수
	
	// TODO 로비 위젯 클래스 추가

	/* 메인 메뉴 */
public:
	void ShowMainMenu();
	void ShowNickname();
	void ShowHUD();
	void ClearUI();
	// TODO void ShowLobby();
	
	UFUNCTION(Server, Reliable)
	void Server_CheckNickname(const FString& Nickname);
	UFUNCTION(Client, Reliable)
	void Client_NicknameCheckResult(bool bSuccess);
	
	/* Chat System */
public:
	void SetChatMessageString(const FString& InChatMessageString);
	void PrintChatMessageString(const FChatMessage& InChatMessage);
	
	void HUDOpenChatInput();
	void HUDCloseChatInput();
	
	bool GetIsChatInputOpen() const {return bIsChatInputOpen;}
	void SetIsChatInputOpen(bool InIsChatInputOpen) {bIsChatInputOpen = InIsChatInputOpen;}
	
private:
	bool bIsChatInputOpen = false;
	
protected:
	UFUNCTION(Server, Reliable)
	void ServerSendChatMessage(const FString& InChatMessage);
};
