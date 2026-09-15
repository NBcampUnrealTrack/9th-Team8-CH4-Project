#include "P48ResultUIWidget.h"
#include "P48RankingData.h"
#include "P48ResultRow.h"

#include "Components/Button.h"
#include "Components/VerticalBox.h"

#include "Project48/Game/P48GameStateBase.h"
#include "Project48/Character/P48PlayerState.h"

#include "Algo/Sort.h"

void UP48ResultUIWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (IsValid(Button_Confirm) == true)
	{
		Button_Confirm->OnClicked.AddDynamic(this, &UP48ResultUIWidget::OnConfirmClicked);
	}
}

void UP48ResultUIWidget::OnConfirmClicked()
{
	UE_LOG(LogTemp, Error, TEXT("로비로 가는 버튼 눌림"));
}

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

    // GameState의 PlayerArray에서 P48PlayerState만 가져온다.
    TArray<AP48PlayerState*> Players;

    for (APlayerState* PlayerState : GS->PlayerArray)
    {
        AP48PlayerState* P48PS = Cast<AP48PlayerState>(PlayerState);

        if (IsValid(P48PS) == false)
        {
            continue;
        }

        // 현재 Match 참가자만 결과에 표시
        if (!P48PS->IsMatchParticipant())
        {
            continue;
        }

        Players.Add(P48PS);
    }

    // 승리 횟수가 높은 순서대로 정렬
    Players.Sort([](const AP48PlayerState& A, const AP48PlayerState& B)
        {
            if (A.GetRoundWinCount() != B.GetRoundWinCount())
            {
                return A.GetRoundWinCount() > B.GetRoundWinCount();
            }

            // 승리 횟수가 같다면 이름 기준 정렬
            return A.GetNickname() < B.GetNickname();
        });

    // RankingData 생성
    TArray<FP48RankingData> RankingData;
    RankingData.Reserve(Players.Num());

    for (int32 Index = 0; Index < Players.Num(); ++Index)
    {
        AP48PlayerState* Player = Players[Index];
        if (IsValid(Player) == false)
        {
            continue;
        }

        FP48RankingData Data;

        Data.Rank = Index + 1;
        Data.Nickname = Player->GetNickname();
        Data.WinCount = Player->GetRoundWinCount();

        // 현재 클라이언트의 PlayerState와 비교
        Data.bIsMe = (Player == MyPlayerState);

        RankingData.Add(Data);

        UE_LOG(LogTemp, Warning, TEXT("[ResultUI] Rank=%d Nickname=%s Wins=%d IsMe=%s"),
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
			
			/*UP48ResultRow* WinnerRow = CreateWidget<UP48ResultRow>(GetOwningPlayer(),RankingWinnerClass);
			if (IsValid(WinnerRow) == false)
			{
				continue;
			}*/
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