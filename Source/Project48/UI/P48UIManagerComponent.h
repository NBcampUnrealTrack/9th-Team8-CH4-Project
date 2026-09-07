#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "P48UIManagerComponent.generated.h"

struct FChatMessage;
class UP48ChatInput;
class UP48ChatWidget;
class UP48HUD;
class AHSPlayerController;

UCLASS( Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECT48_API UP48UIManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UP48UIManagerComponent();
	
	virtual void BeginPlay() override;
	
	void SetChatMessageString(const FString& InChatMessageString);
	void PrintChatMessageString(const FChatMessage& InChatMessage);

protected:
	void OpenChatInput();
	void CloseChatInput();

	//void ToggleChatInput();
	AHSPlayerController* UIManagerGetController();
	
protected:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UP48ChatWidget> ChatWidgetClass; // UP48ChatWidget 기반으로 만들어진 WidgetBlueprint의 클래스
	UPROPERTY()
	TObjectPtr<UP48ChatWidget> ChatWidgetInstance; // 실제로 생성된 채팅 위젯 객체를 저장하는 변수
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UP48HUD> HUDClass;
	UPROPERTY()
	TObjectPtr<UP48HUD> HUDInstance;
	
	bool bIsChatInputOpen = false;
	
protected:
	UFUNCTION(Server, Reliable)
	void ServerSendChatMessage(const FString& InChatMessage);
};
