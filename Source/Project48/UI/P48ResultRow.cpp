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
	
	if (Data.Score == 1)
	{
		if (Image_Crown1)
		{
			Image_Crown1->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else if (Data.Score == 2)
	{
		if (Image_Crown2)
		{
			Image_Crown2->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else if (Data.Score == 3)
	{
		if (Image_Crown3)
		{
			Image_Crown3->SetVisibility(ESlateVisibility::Visible);
		}
	}
}