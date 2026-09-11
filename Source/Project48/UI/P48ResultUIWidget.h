#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48ResultUIWidget.generated.h"

class UButton;

UCLASS()
class PROJECT48_API UP48ResultUIWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Confirm;
	
private:
	UFUNCTION()
	void OnConfirmClicked();
};
