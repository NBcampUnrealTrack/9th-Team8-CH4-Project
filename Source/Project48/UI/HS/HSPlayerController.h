#pragma once

#include "CoreMinimal.h"
#include "Project48/Character/P48PlayerController.h"
#include "HSPlayerController.generated.h"

struct FChatMessage;
class UP48ChatInput;
class UP48ChatWidget;
class UP48CombatWidget;

UCLASS()
class PROJECT48_API AHSPlayerController : public AP48PlayerController
{
	GENERATED_BODY()
	
public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	
	void SetChatMessageString(const FString& InChatMessageString);
	void PrintChatMessageString(const FChatMessage& InChatMessage);

protected:
	void OpenChatInput();
	void CloseChatInput();

	void ToggleChatInput();
	
protected:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UP48ChatWidget> ChatWidgetClass; // UP48ChatWidget 기반으로 만들어진 WidgetBlueprint의 클래스
	UPROPERTY()
	TObjectPtr<UP48ChatWidget> ChatWidgetInstance; // 실제로 생성된 채팅 위젯 객체를 저장하는 변수
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UP48CombatWidget> CombatWidgetClass;
	UPROPERTY()
	TObjectPtr<UP48CombatWidget> CombatWidgetInstance;
	
	bool bIsChatInputOpen = false;
	
protected:
	UFUNCTION(Server, Reliable)
	void ServerSendChatMessage(const FString& InChatMessage);
};
