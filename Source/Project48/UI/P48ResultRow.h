#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48ResultRow.generated.h"

class UTextBlock;
class UImage;

UCLASS()
class PROJECT48_API UP48ResultRow : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetRankingData(
		int32 Rank,
		const FString& Nickname,
		int32 Score,
		bool bIsMe,
		bool bIsWinner
	);

protected:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Rank;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Nickname;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Score;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_YOU;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Crown1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Crown2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Crown3;
};
