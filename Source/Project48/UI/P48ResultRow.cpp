#include "P48ResultRow.h"
#include "P48RankingData.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UP48ResultRow::SetRankingData(const FP48RankingData& Data)
{
	if (TextBlock_Rank)
	{
		TextBlock_Rank->SetText(FText::AsNumber(Data.Rank));
	}
	if (TextBlock_Nickname)
	{
		TextBlock_Nickname->SetText(FText::FromString(Data.Nickname));
	}
	if (Image_YOU)
	{
		Image_YOU->SetVisibility(
		Data.bIsMe
		? ESlateVisibility::Visible
		: ESlateVisibility::Collapsed);
	}
	if (IsValid(Image_Crown1) == true)
	{
		Image_Crown1->SetBrushFromTexture(DonutTexture);
	}
	if (IsValid(Image_Crown2) == true)
	{
		Image_Crown2->SetBrushFromTexture(DonutTexture);
	}
	
	if (Data.WinCount >= 1)
	{
		if (IsValid(Image_Crown1) == true)
		{
			Image_Crown1->SetBrushFromTexture(CrownTexture);
		}
	}
	if (Data.WinCount >= 2)
	{
		if (IsValid(Image_Crown2) == true)
		{
			Image_Crown2->SetBrushFromTexture(CrownTexture);
		}
	}
}