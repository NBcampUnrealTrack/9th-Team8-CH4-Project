#include "P48PlayerNameWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

void UP48PlayerNameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ProgressBar_GroggyGauge)
	{
		ProgressBar_GroggyGauge->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UP48PlayerNameWidget::SetPlayerName(const FString& InPlayerName)
{
	if (IsValid(TextBlock_PlayerNameText) == false) return;
	
	TextBlock_PlayerNameText->SetText(FText::FromString(InPlayerName));
}

void UP48PlayerNameWidget::UpdateGroggy(float CurrentGroggy, float MaxGroggy)
{
	if (IsValid(ProgressBar_GroggyGauge) == false) return;

	if (CurrentGroggy <= 0.0f)
	{
		ProgressBar_GroggyGauge->SetPercent(0.0f);
		ProgressBar_GroggyGauge->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	
	ProgressBar_GroggyGauge->SetVisibility(ESlateVisibility::Visible);
	
	const float Percent = CurrentGroggy / MaxGroggy;
	
	ProgressBar_GroggyGauge->SetPercent(Percent);
}
