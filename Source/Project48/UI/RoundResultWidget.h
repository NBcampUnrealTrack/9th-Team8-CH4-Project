#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RoundResultWidget.generated.h"

class AP48PlayerState;
class UImage;

UCLASS()
class PROJECT48_API URoundResultWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleRoundResultChanged(AP48PlayerState* Winner, bool bIsDraw);
	
	// Image_RoundResult
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_RoundResult; // "승리" / "패배" / "무승부"
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> DrawTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> VictoryTexture;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> DefeatTexture;

private:
	void ShowResult(AP48PlayerState* Winner, bool bIsDraw);
};
