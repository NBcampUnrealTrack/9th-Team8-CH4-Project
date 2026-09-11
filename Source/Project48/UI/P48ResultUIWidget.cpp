#include "P48ResultUIWidget.h"
#include "Components/Button.h"

void UP48ResultUIWidget::NativeConstruct()
{
	if (IsValid(Button_Confirm) == true)
	{
		Button_Confirm->OnClicked.AddDynamic(this, &UP48ResultUIWidget::OnConfirmClicked);
	}
}

void UP48ResultUIWidget::OnConfirmClicked()
{
	UE_LOG(LogTemp, Error, TEXT("로비로 가는 버튼 눌림"));
}
