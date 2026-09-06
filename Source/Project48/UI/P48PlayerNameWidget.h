#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48PlayerNameWidget.generated.h"

class UTextBlock;

UCLASS()
class PROJECT48_API UP48PlayerNameWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void SetPlayerName(const FString& InPlayerName);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock_PlayerNameText;
};
