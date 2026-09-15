#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48ResultUIWidget.generated.h"

class UVerticalBox;
class UP48ResultRow;
class UButton;
struct FP48RankingData;

UCLASS()
class PROJECT48_API UP48ResultUIWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 서버/PlayerState 데이터 수신 시 호출
	void RefreshRanking(const TArray<FP48RankingData>& RankingData);
	
	void BuildAndRefreshRanking();
	
protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Confirm;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> VerticalBox_RankingBox;
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UP48ResultRow> RankingRowClass;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UP48ResultRow> ResultRow_Winner;
	
private:
	UFUNCTION()
	void OnConfirmClicked();
};
