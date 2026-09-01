#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48ChatMessage.generated.h"

class UTextBlock;

UCLASS()
class PROJECT48_API UP48ChatMessage : public UUserWidget
{
	GENERATED_BODY()
	
public:

	void SetChatMessage(const FString& InChatMessage);

protected:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock_ChatMessage;
};
