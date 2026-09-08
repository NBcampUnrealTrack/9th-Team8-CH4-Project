#include "P48MainMenuWidget.h"
#include "P48InputNicknameWidget.h"
#include "Components/Button.h"
#include "Project48/UI/P48UIManagerComponent.h"

void UP48MainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(Button_GameStart) == true)
	{
		Button_GameStart->OnClicked.AddDynamic(this, &UP48MainMenuWidget::OnGameStartClicked);
	}
}

void UP48MainMenuWidget::OnGameStartClicked()
{
	UP48UIManagerComponent* UIManager = GetOwningPlayer()->FindComponentByClass<UP48UIManagerComponent>();
	if (IsValid(UIManager) == false) return;

	UIManager->ShowNickname();
}
