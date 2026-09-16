#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48InputNicknameWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

UCLASS()
class PROJECT48_API UP48InputNicknameWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Confirm;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableText_InputNickname;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> EditableText_InputUserID;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock>TextBlock_Error;

private:
	UFUNCTION()
	void OnConfirmClicked();
	
public:
	UFUNCTION()
	void SetErrorText(const FString& InText);
	
	UPROPERTY (EditDefaultsOnly, BlueprintReadOnly, Category = "URL")
	FString URL = TEXT("25.5.238.57:17777");
	//FString URL = TEXT("127.0.0.1:17777");
};
