#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48ChatWidget.generated.h"

struct FChatMessage;
class UVerticalBox;
class UScrollBox;
class UP48ChatMessage;
class UP48ChatInput;

UCLASS()
class PROJECT48_API UP48ChatWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;
	
	void OpenChatInput();
	void CloseChatInput();
	void AddChatMessage(const FChatMessage& InChatMessage);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ScrollBox_Chat;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> VerticalBox_ChatMessages;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UP48ChatInput> ChatInput;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UP48ChatMessage> ChatMessageClass;
	
	UPROPERTY(EditDefaultsOnly)
	int32 MaxChatMessageCount = 10; // 최대 메시지 개수 제한
};
