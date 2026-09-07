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
	
	void HUDOpenChatInput();
	void HUDCloseChatInput();
	
	bool GetIsChatInputOpen() const {return bIsChatInputOpen;}
	void SetIsChatInputOpen(bool InIsChatInputOpen) {bIsChatInputOpen = InIsChatInputOpen;}
protected:
	//void ToggleChatInput();
	//AHSPlayerController* UIManagerGetController();
	
protected:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UP48HUD> HUDClass; // UP48HUD 기반으로 만들어진 WidgetBlueprint의 클래스
	UPROPERTY()
	TObjectPtr<UP48HUD> HUDInstance; // 실제로 생성된 채팅 위젯 객체를 저장하는 변수
	
private:
	bool bIsChatInputOpen = false;
	
protected:
	UFUNCTION(Server, Reliable)
	void ServerSendChatMessage(const FString& InChatMessage);
};
