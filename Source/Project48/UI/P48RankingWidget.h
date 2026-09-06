#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "P48RankingWidget.generated.h"

class AP48PlayerState;
class AP48GameStateBase;
class UTextBlock;

UCLASS()
class PROJECT48_API UP48RankingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	// 랭킹 정보 갱신
	UFUNCTION(BlueprintCallable, Category = "Ranking")
	void UpdateRanking();
	
protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock_FirstPlayerName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock_FirstPlayerWin;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock_MyPlayerName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock_MyPlayerWin;

private:
	AP48GameStateBase* GetP48GameState() const;
	AP48PlayerState* GetMyPlayerState() const;
	AP48PlayerState* FindFirstPlayer() const;
};
