#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48RoundWidget.generated.h"

class UVerticalBox;
class UP48ResultRow;

UCLASS()
class PROJECT48_API UP48RoundWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> VerticalBox_RankingBox;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UP48ResultRow> RankingRowClass;
};
