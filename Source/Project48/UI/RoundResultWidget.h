#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RoundResultWidget.generated.h"

class UTextBlock;
class AP48PlayerState;

UCLASS()
class PROJECT48_API URoundResultWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleRoundResultChanged(AP48PlayerState* Winner, bool bIsDraw);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock_Result; // "승리" / "패배" / "무승부"

private:
	void ShowResult(AP48PlayerState* Winner, bool bIsDraw);
};
