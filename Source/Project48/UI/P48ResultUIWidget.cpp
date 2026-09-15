#include "P48ResultUIWidget.h"
#include "P48RankingData.h"
#include "P48ResultRow.h"

#include "Components/Button.h"
#include "Components/VerticalBox.h"

#include "Project48/Game/P48GameStateBase.h"
#include "Project48/Character/P48PlayerState.h"
#include "Project48/Lobby/P48LobbyTravelSubsystem.h"

#include "Algo/Sort.h"

void UP48ResultUIWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	/*if (IsValid(Button_Confirm) == true)
	{
		Button_Confirm->OnClicked.AddDynamic(this, &UP48ResultUIWidget::OnConfirmClicked);
	}*/
}

/*void UP48ResultUIWidget::OnConfirmClicked()
{
	UE_LOG(LogTemp, Log, TEXT("로비로 가는 버튼 눌림"));
}*/

void UP48ResultUIWidget::BuildAndRefreshRanking()
{
    if (IsValid(VerticalBox_RankingBox) == false)
    {
        UE_LOG(LogTemp, Error, TEXT("[ResultUI] VerticalBox_RankingBox is invalid."));
        return;
    }

    UWorld* World = GetWorld();
    if (IsValid(World) == false)
    {
        return;
    }

    AP48GameStateBase* GS = World->GetGameState<AP48GameStateBase>();
    if (IsValid(GS) == false)
    {
        UE_LOG(LogTemp, Error, TEXT("[ResultUI] GameState is invalid."));
        return;
    }

	//최종 데이터가 준비되지 않았다면 UI를 만들지 않는다.
	if (!GS->HasFinalRankingData())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ResultUI] FinalRankingData is not ready."));
		return;
	}
	
    APlayerController* OwningPC = GetOwningPlayer();
    if (IsValid(OwningPC) == false)
    {
        UE_LOG(LogTemp, Error, TEXT("[ResultUI] OwningPlayer is invalid."));
        return;
    }

    AP48PlayerState* MyPlayerState = OwningPC->GetPlayerState<AP48PlayerState>();
    if (IsValid(MyPlayerState) == false)
    {
        UE_LOG(LogTemp, Error, TEXT("[ResultUI] My PlayerState is invalid."));
        return;
    }

    // 기존 Row 삭제
    VerticalBox_RankingBox->ClearChildren();

	// 서버가 확정한 최종 랭킹 복사
	TArray<FP48RankingData> RankingData = GS->GetFinalRankingData();

	// 서버 데이터에는 bIsMe가 없으므로 클라이언트에서 자기 자신만 표시
	for (FP48RankingData& Data : RankingData)
	{
		Data.bIsMe = (Data.Nickname == MyPlayerState->GetNickname());

		UE_LOG(LogTemp, Warning, TEXT("[ResultUI] Final Rank=%d Nickname=%s Wins=%d IsMe=%s"),
			Data.Rank,
			*Data.Nickname,
			Data.WinCount,
			Data.bIsMe
			? TEXT("true")
			: TEXT("false"));
	}

	RefreshRanking(RankingData);
}

void UP48ResultUIWidget::RefreshRanking(const TArray<FP48RankingData>& RankingData)
{
	if (IsValid(VerticalBox_RankingBox) == false)
	{
		return;
	}
	// 기존 랭킹 목록 초기화
	VerticalBox_RankingBox->ClearChildren();

	for (const FP48RankingData& Data : RankingData)
	{
		if (Data.Rank == 1)
		{
			if (IsValid(ResultRow_Winner) == false) return;
			
			ResultRow_Winner->SetRankingData(Data);
			ResultRow_Winner->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			if (IsValid(RankingRowClass) == false) return;
			
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