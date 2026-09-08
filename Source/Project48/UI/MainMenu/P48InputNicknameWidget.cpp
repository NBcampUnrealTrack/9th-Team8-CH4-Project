#include "P48InputNicknameWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Project48/UI/P48UIManagerComponent.h"

void UP48InputNicknameWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (IsValid(Button_Confirm) == true)
	{
		Button_Confirm->OnClicked.AddDynamic(this, &UP48InputNicknameWidget::OnConfirmClicked);
	}
}

void UP48InputNicknameWidget::OnConfirmClicked()
{
	if (IsValid(EditableText_InputNickname) == false) return;
	FString Nickname = EditableText_InputNickname->GetText().ToString();

	if (Nickname.IsEmpty() || Nickname.Contains(TEXT(" "))) // 빈칸, 공백 체크
	{
		if (IsValid(TextBlock_Error) == false) return;
		SetErrorText(("Please enter a nickname without spaces."));
		return;
	}

	UP48UIManagerComponent* UIManager = GetOwningPlayer()->FindComponentByClass<UP48UIManagerComponent>();
	if (IsValid(UIManager) == false) return;
	
	UIManager->Server_CheckNickname(Nickname);
	
	UE_LOG(LogTemp, Error, TEXT("확인 버튼 눌림"));
	// 서버에 닉네임 중복 검사 요청
	//PC->Server_CheckNickname(Nickname);
	 // true 면 다음 레벨 오픈하는 함수
	// false 면 에러 텍스트 활성화
}

void UP48InputNicknameWidget::SetErrorText(const FString& InText)
{
	TextBlock_Error->SetText(FText::FromString(InText));
}
