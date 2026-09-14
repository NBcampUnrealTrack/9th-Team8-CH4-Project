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
    UpdateRanking();
}

void UP48RankingWidget::UpdateRanking()
{
    AP48GameStateBase* GameState = GetP48GameState();

    if (!GameState)
    {
        return;
    }

    AP48PlayerState* MyPlayerState = GetMyPlayerState();

    if (!MyPlayerState)
    {
        return;
    }

    AP48PlayerState* FirstPlayer = FindFirstPlayer();

    if (!FirstPlayer)
    {
        return;
    }

    // -------------------------
    // 1등 정보
    // -------------------------

    if (TextBlock_FirstPlayerName)
    {
        TextBlock_FirstPlayerName->SetText(FText::FromString(FirstPlayer->GetNickname()));
    }

    if (TextBlock_FirstPlayerWin)
    {
        TextBlock_FirstPlayerWin->SetText(FText::AsNumber(FirstPlayer->GetRoundWinCount()));
    }


    // -------------------------
    // 내 정보
    // -------------------------

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

    return GetWorld()->GetGameState<AP48GameStateBase>();
}

AP48PlayerState* UP48RankingWidget::GetMyPlayerState() const
{
    APlayerController* PC = GetOwningPlayer();

    if (!PC)
    {
        return nullptr;
    }

    return PC->GetPlayerState<AP48PlayerState>();
}

AP48PlayerState* UP48RankingWidget::FindFirstPlayer() const
{
    AP48GameStateBase* GameState = GetP48GameState();

    if (!GameState)
    {
        return nullptr;
    }

    AP48PlayerState* FirstPlayer = nullptr;

    int32 HighestWinCount = -1;

    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        AP48PlayerState* P48PlayerState =
            Cast<AP48PlayerState>(PlayerState);

        if (!P48PlayerState)
        {
            continue;
        }

        if (P48PlayerState->GetRoundWinCount() > HighestWinCount)
        {
            HighestWinCount = P48PlayerState->GetRoundWinCount();

            FirstPlayer = P48PlayerState;
        }
    }

    return FirstPlayer;
}