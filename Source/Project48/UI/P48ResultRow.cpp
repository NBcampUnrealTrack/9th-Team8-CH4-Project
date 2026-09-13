#include "P48ResultRow.h"
#include "Components/TextBlock.h"

void UP48ResultRow::SetRankingData(
	int32 Rank,
	const FString& Nickname,
	int32 Score,
	bool bIsMe,
	bool bIsWinner)
{
	Text_Rank->SetText(
		FText::AsNumber(Rank)
	);

	Text_Nickname->SetText(
		FText::FromString(Nickname)
	);

	Text_Score->SetText(
		FText::AsNumber(Score)
	);

	Text_YOU->SetVisibility(
		bIsMe
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed
	);
}