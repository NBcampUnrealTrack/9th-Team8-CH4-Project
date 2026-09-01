#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48ChatWidget.generated.h"

class UVerticalBox;
class UScrollBox;
class UP48ChatMessage;

UCLASS()
class PROJECT48_API UP48ChatWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:

	void AddChatMessage(const FString& InChatMessage);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ScrollBox_Chat;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> VerticalBox_ChatMessages;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UP48ChatMessage> ChatMessageClass;
	
	UPROPERTY(EditDefaultsOnly)
	int32 MaxChatMessageCount = 10; // 최대 메시지 개수 제한
};
