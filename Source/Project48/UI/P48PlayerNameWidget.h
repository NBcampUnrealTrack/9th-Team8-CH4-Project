#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48PlayerNameWidget.generated.h"

class UTextBlock;
class UProgressBar;

UCLASS()
class PROJECT48_API UP48PlayerNameWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;
	
	UFUNCTION(BlueprintCallable)
	void SetPlayerName(const FString& InPlayerName);
	UFUNCTION(BlueprintCallable)
	void UpdateGroggy(float CurrentGroggy, float MaxGroggy);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock_PlayerNameText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ProgressBar_GroggyGauge;
};
