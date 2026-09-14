#include "P48ResultUIWidget.h"
#include "P48RankingData.h"
#include "P48ResultRow.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"

void UP48ResultUIWidget::NativeConstruct()
{
	if (IsValid(Button_Confirm) == true)
	{
		Button_Confirm->OnClicked.AddDynamic(this, &UP48ResultUIWidget::OnConfirmClicked);
	}
}

void UP48ResultUIWidget::OnConfirmClicked()
{
	UE_LOG(LogTemp, Error, TEXT("로비로 가는 버튼 눌림"));
}

void UP48ResultUIWidget::RefreshRanking(const TArray<FP48RankingData>& RankingData)
{
	// 기존 랭킹 목록 초기화
	VerticalBox_RankingBox->ClearChildren();

	for (const FP48RankingData& Data : RankingData)
	{
		if (Data.Rank == 1)
		{
			UP48ResultRow* WinnerRow = CreateWidget<UP48ResultRow>(GetOwningPlayer(),RankingWinnerClass);
			if (IsValid(WinnerRow) == false)
			{
				continue;
			}
			WinnerRow->SetRankingData(Data);
		}
		else
		{
			UP48ResultRow* Row = CreateWidget<UP48ResultRow>(GetOwningPlayer(),RankingRowClass);

			if (IsValid(Row) == false)
			{
				continue;
			}

			Row->SetRankingData(Data);
			VerticalBox_RankingBox->AddChild(Row);
		}
	}
}

/*void UP48ResultUIWidget::UpdateRankingBoard(TArray<FP48RankingData> PlayerScores)
{
	if (!RankingRowClass || !VerticalBox_RankingBox) return;

	// 기존 랭킹 목록 초기화
	VerticalBox_RankingBox->ClearChildren();

	// 1. 순위(Rank) 기준 오름차순 정렬 (1등부터 순서대로)
	Algo::Sort(PlayerScores, [](const FP48RankingData& A, const FP48RankingData& B) {
		return A.Rank < B.Rank;});

	// 2. 순서대로 VerticalBox에 자식 위젯 추가
	for (int32 Index = 0; Index < PlayerScores.Num(); ++Index)
	{
		FP48RankingData& Data = PlayerScores[Index];
        
		// 1등 여부 판단
		if (Data.Rank == 1)
		{
			Data.bIs1stPlace = true;
		}

		UP48ResultRow* RowWidget = CreateWidget<UP48ResultRow>(this, RankingRowClass);
		if (RowWidget)
		{
			RowWidget->SetRankingData(Data);
			VerticalBox_RankingBox->AddChildToVerticalBox(RowWidget);
		}
	}
}*/