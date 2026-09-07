#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48MainMenuWidget.generated.h"

class UButton;

UCLASS()
class PROJECT48_API UP48MainMenuWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_GameStart;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_GameQuit;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Settings;

private:
	UFUNCTION()
	void OnGameStartClicked();
	UFUNCTION()
	void OnGameQuitClicked();
	
};
