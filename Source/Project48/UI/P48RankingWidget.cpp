#include "P48RankingWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Project48/Game/P48GameStateBase.h"
#include "Project48/Character/P48PlayerState.h"

void UP48RankingWidget::NativeConstruct()
{
    Super::NativeConstruct();

    AP48GameStateBase* GS = GetP48GameState();
    if (IsValid(GS) == false) return;

    GS->OnRoundResultChanged.AddDynamic(this, &UP48RankingWidget::HandleRoundResultChanged);
    GS->OnCurrentRoundChanged.AddDynamic(this, &UP48RankingWidget::HandleCurrentRoundChanged);
    
    UpdateRanking();
}

void UP48RankingWidget::NativeDestruct()
{
    AP48GameStateBase* GS = GetP48GameState();
    if (IsValid(GS) == false) return;
    
    GS->OnRoundResultChanged.RemoveDynamic(this, &UP48RankingWidget::HandleRoundResultChanged);
    GS->OnCurrentRoundChanged.RemoveDynamic(this, &UP48RankingWidget::HandleCurrentRoundChanged);

    Super::NativeDestruct();
}

void UP48RankingWidget::HandleCurrentRoundChanged(int32 NewRound)
{
    UpdateRanking();
}

void UP48RankingWidget::HandleRoundResultChanged(AP48PlayerState* Winner, bool bIsDraw)
{
    // ResetRoundResult() 등으로 결과가 없는 상태라면 무시
    if (IsValid(Winner) == false && bIsDraw == false) return;

    UpdateRanking();
}

void UP48RankingWidget::UpdateRanking()
{
    AP48PlayerState* MyPlayerState = GetMyPlayerState();
    if (IsValid(MyPlayerState) == false) return;
    
    AP48GameStateBase* GameState = GetP48GameState();
    if (IsValid(GameState) == false) return;
    
    const int32 CurrentRound = GameState->CurrentRound;

    UE_LOG(LogTemp, Warning, TEXT("[RankingWidget] UpdateRanking - CurrentRound = %d"),CurrentRound);
    
    if (CurrentRound == 1)
    {
        // 1등 정보 숨기기
        if (TextBlock_FirstPlayerName)
        {
            TextBlock_FirstPlayerName->SetVisibility(ESlateVisibility::Collapsed);
        }

        if (TextBlock_FirstPlayerWin)
        {
            TextBlock_FirstPlayerWin->SetVisibility(ESlateVisibility::Collapsed);
        }
        
        // 내 정보 표시
        if (TextBlock_MyPlayerName)
        {
            TextBlock_MyPlayerName->SetVisibility(ESlateVisibility::Visible);
            TextBlock_MyPlayerName->SetText(FText::FromString(MyPlayerState->GetNickname()));
        }

        if (TextBlock_MyPlayerWin)
        {
            TextBlock_MyPlayerWin->SetVisibility(ESlateVisibility::Visible);
            TextBlock_MyPlayerWin->SetText(FText::AsNumber(MyPlayerState->GetRoundWinCount()));
        }

        return;
    }
    
    AP48PlayerState* FirstPlayerPS = GetFirstPlayerState();
    if (IsValid(FirstPlayerPS) == false) return;
    
    // 1등 정보
    if (TextBlock_FirstPlayerName)
    {
        TextBlock_FirstPlayerName->SetVisibility(ESlateVisibility::Visible);
        TextBlock_FirstPlayerName->SetText(FText::FromString(FirstPlayerPS->GetNickname()));
    }

    if (TextBlock_FirstPlayerWin)
    {
        TextBlock_FirstPlayerWin->SetVisibility(ESlateVisibility::Visible);
        TextBlock_FirstPlayerWin->SetText(FText::AsNumber(FirstPlayerPS->GetRoundWinCount()));
    }
    
    // 내 정보 표시
    if (TextBlock_MyPlayerName)
    {
        TextBlock_MyPlayerName->SetVisibility(ESlateVisibility::Visible);
        TextBlock_MyPlayerName->SetText(FText::FromString(MyPlayerState->GetNickname()));
    }

    if (TextBlock_MyPlayerWin)
    {
        TextBlock_MyPlayerWin->SetVisibility(ESlateVisibility::Visible);
        TextBlock_MyPlayerWin->SetText(FText::AsNumber(MyPlayerState->GetRoundWinCount()));
    }
}

AP48GameStateBase* UP48RankingWidget::GetP48GameState() const
{
    if (!GetWorld())
    {
        return nullptr;
    }

    AP48GameStateBase* GS = GetWorld()->GetGameState<AP48GameStateBase>();
    if (IsValid(GS) == false)
    {
        UE_LOG(LogTemp, Error, TEXT("RW GS 생성 실패"));
        return nullptr;
    }
    
    return GS;
}

AP48PlayerState* UP48RankingWidget::GetMyPlayerState() const
{
    APlayerController* PC = GetOwningPlayer();
    if (IsValid(PC) == false)
    {
        UE_LOG(LogTemp, Error, TEXT("RW PC 생성 실패"));
        return nullptr;
    }

    AP48PlayerState* MyPS = PC->GetPlayerState<AP48PlayerState>();
    if (IsValid(MyPS) == false)
    {
        UE_LOG(LogTemp, Error, TEXT("RW MyPS 생성 실패"));
        return nullptr;
    }
    
    return MyPS;
}

AP48PlayerState* UP48RankingWidget::GetFirstPlayerState() const
{
    AP48GameStateBase* GameState = GetP48GameState();
    if (IsValid(GameState) == false)
    {
        UE_LOG(LogTemp, Error, TEXT("RW GameState 생성 실패"));
        return nullptr;
    }

    AP48PlayerState* FirstPlayerPS = nullptr;
    int32 HighestWinCount = -1;

    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        AP48PlayerState* P48PlayerState = Cast<AP48PlayerState>(PlayerState);
        if (IsValid(P48PlayerState) == false)
        {
            continue;
        }

        if (P48PlayerState->GetRoundWinCount() > HighestWinCount)
        {
            HighestWinCount = P48PlayerState->GetRoundWinCount();

            FirstPlayerPS = P48PlayerState;
        }
    }

    return FirstPlayerPS;
}