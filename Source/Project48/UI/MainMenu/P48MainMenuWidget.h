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
	TObjectPtr<UButton> GameStartButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> GameQuitButton;

private:
	UFUNCTION()
	void OnGameStartClicked();
	UFUNCTION()
	void OnGameQuitClicked();
	
};
