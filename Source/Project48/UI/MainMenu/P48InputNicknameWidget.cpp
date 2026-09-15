#include "P48InputNicknameWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UP48InputNicknameWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (IsValid(Button_Confirm) == true)
	{
		Button_Confirm->OnClicked.AddDynamic(this, &UP48InputNicknameWidget::OnConfirmClicked);
	}
}

void UP48InputNicknameWidget::OnConfirmClicked()
{
	if (IsValid(EditableText_InputNickname) == false) return;
	FString Nickname = EditableText_InputNickname->GetText().ToString().TrimStartAndEnd();
	if (IsValid(EditableText_InputUserID) == false) return;
	FString UserID = EditableText_InputUserID->GetText().ToString().TrimStartAndEnd();
	
	APlayerController* OwningPlayer = GetOwningPlayer();
	if (IsValid(OwningPlayer) == false) return;
	
	URL = FString::Printf(TEXT("127.0.0.1:17777?UserID=%s?Nickname=%s"), *UserID, *Nickname);
	
	OwningPlayer->ClientTravel(URL, ETravelType::TRAVEL_Absolute);
	
	UE_LOG(LogTemp, Log, TEXT("확인 버튼 눌림"));
}

void UP48InputNicknameWidget::SetErrorText(const FString& InText)
{
	TextBlock_Error->SetText(FText::FromString(InText));
}
