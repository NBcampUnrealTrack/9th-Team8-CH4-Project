#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48ResultRow.generated.h"

class UTextBlock;
class UImage;
struct FP48RankingData;

UCLASS()
class PROJECT48_API UP48ResultRow : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetRankingData(const FP48RankingData& Data);

protected:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock_Rank;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock_Nickname;

	/*UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Score;*/

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_YOU;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Crown1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Crown2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Crown3;
};
