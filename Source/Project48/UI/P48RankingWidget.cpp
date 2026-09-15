#include "P48RankingWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Project48/Game/P48GameStateBase.h"
#include "Project48/Character/P48PlayerState.h"

void UP48RankingWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 위젯 생성 직후 한 번 갱신
    //UpdateRanking();
}

void UP48RankingWidget::UpdateRanking()
{
    AP48PlayerState* MyPlayerState = GetMyPlayerState();
    
    AP48PlayerState* FirstPlayerPS = GetFirstPlayerState();
    
    // 1등 정보
    if (TextBlock_FirstPlayerName)
    {
        TextBlock_FirstPlayerName->SetText(FText::FromString(FirstPlayerPS->GetNickname()));
    }

    if (TextBlock_FirstPlayerWin)
    {
        TextBlock_FirstPlayerWin->SetText(FText::AsNumber(FirstPlayerPS->GetRoundWinCount()));
    }

    
    // 내 정보
    if (TextBlock_MyPlayerName)
    {
        TextBlock_MyPlayerName->SetText(FText::FromString(MyPlayerState->GetNickname()));
    }

    if (TextBlock_MyPlayerWin)
    {
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