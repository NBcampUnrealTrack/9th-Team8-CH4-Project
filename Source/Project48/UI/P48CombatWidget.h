#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48CombatWidget.generated.h"

class UP48ChatWidget;
class UP48RankingWidget;

UCLASS()
class PROJECT48_API UP48CombatWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UP48ChatWidget> ChatWidget;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UP48RankingWidget> RankingWidget;
};
