#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48ChatInput.generated.h"

class UEditableTextBox;

UCLASS()
class PROJECT48_API UP48ChatInput : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	UFUNCTION()
	void OnChatInputTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	
public:
	UPROPERTY(meta = (BindWidget)) // UMG와 C++ 연결
	TObjectPtr<UEditableTextBox> EditableTextBox_ChatInput; // 이름이 EditableTextBox_ChatInput 인것과 연결
};
