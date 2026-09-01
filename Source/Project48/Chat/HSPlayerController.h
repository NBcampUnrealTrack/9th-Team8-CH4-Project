#pragma once

#include "CoreMinimal.h"
#include "Project48/Character/P48PlayerController.h"
#include "HSPlayerController.generated.h"

class UP48ChatInput;
class UP48ChatWidget;

UCLASS()
class PROJECT48_API AHSPlayerController : public AP48PlayerController
{
	GENERATED_BODY()
	
public:
	virtual void BeginPlay() override;
	
	void SetChatMessageString(const FString& InChatMessageString);
	void PrintChatMessageString(const FString& InChatMessageString);

protected:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UP48ChatWidget> ChatWidgetClass; // UP48ChatWidget 기반으로 만들어진 WidgetBlueprint의 클래스
	UPROPERTY()
	TObjectPtr<UP48ChatWidget> ChatWidgetInstance; // 실제로 생성된 채팅 위젯 객체를 저장하는 변수
};
