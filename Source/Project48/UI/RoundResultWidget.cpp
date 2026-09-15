#include "RoundResultWidget.h"
#include "Components/TextBlock.h"
#include "Project48/Game/P48GameStateBase.h"
#include "Project48/Character/P48PlayerState.h"

void URoundResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::Collapsed);

	if (AP48GameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AP48GameStateBase>() : nullptr)
	{
		GS->OnRoundResultChanged.AddDynamic(this, &URoundResultWidget::HandleRoundResultChanged);

		// 위젯이 생성된 시점에 이미 결과가 나와있는 경우(재접속 등) 대비
		if (GS->HasRoundResult())
		{
			ShowResult(GS->GetRoundWinner(), GS->IsRoundDraw());
		}
	}
}

void URoundResultWidget::NativeDestruct()
{
	if (AP48GameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AP48GameStateBase>() : nullptr)
	{
		GS->OnRoundResultChanged.RemoveDynamic(this, &URoundResultWidget::HandleRoundResultChanged);
	}
	Super::NativeDestruct();
}

void URoundResultWidget::HandleRoundResultChanged(AP48PlayerState* Winner, bool bIsDraw)
{
	// ResetRoundResult()가 호출되면 Winner=nullptr, bIsDraw=false로 브로드캐스트되므로
	// "결과 없음" 상태에서는 배너를 감춘다.
	if (IsValid(Winner) == false && bIsDraw == false)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	ShowResult(Winner, bIsDraw);
}

void URoundResultWidget::ShowResult(AP48PlayerState* Winner, bool bIsDraw)
{
	if (IsValid(TextBlock_Result) == false) return;

	const AP48PlayerState* LocalPS = Cast<AP48PlayerState>(GetOwningPlayerState());

	if (bIsDraw)
	{
		TextBlock_Result->SetText(FText::FromString(TEXT("Draw")));
	}
	else if (IsValid(Winner) && Winner == LocalPS)
	{
		TextBlock_Result->SetText(FText::FromString(TEXT("Victory")));
	}
	else
	{
		TextBlock_Result->SetText(FText::FromString(TEXT("Defeat")));
	}

	SetVisibility(ESlateVisibility::Visible);
}