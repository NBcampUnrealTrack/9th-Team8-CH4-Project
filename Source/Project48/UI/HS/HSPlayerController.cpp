#include "HSPlayerController.h"
#include "Project48/UI/P48UIManagerComponent.h"
#include "Project48/Database/P48NicknameDatabaseSubsystem.h"

/*AHSPlayerController::AHSPlayerController()
{
	UIManagerComp = CreateDefaultSubobject<UP48UIManagerComponent>(TEXT("UIManagerComponent"));
}

void AHSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsValid(InputComponent))
	{
		InputComponent->BindKey(
			EKeys::Enter,
			IE_Pressed,
			this,
			&AHSPlayerController::ToggleChatInput
		);
	}
}

void AHSPlayerController::ToggleChatInput()
{
	UP48UIManagerComponent* UIManager = FindComponentByClass<UP48UIManagerComponent>();
	if (IsValid(UIManager) == false) return;
	
	if (UIManager->GetIsChatInputOpen() == true) return;

	UIManager->HUDOpenChatInput();
}

void AHSPlayerController::ClientWasKicked_Implementation(const FText& KickReason)
{
	Super::ClientWasKicked_Implementation(KickReason);
	
	UP48UIManagerComponent* UIManager = FindComponentByClass<UP48UIManagerComponent>();
	if (IsValid(UIManager) == false) return;
	
	FString FailureMessage = KickReason.ToString();
	
	if (FailureMessage.Equals(TEXT("DuplicateNickname")))
	{
		UIManager->ShowConnectionRejectedMessage(EP48NicknameRegistrationResult::DuplicateNickname);
	}
	else if (FailureMessage.Equals(TEXT("InvalidFormat")))
	{
		UIManager->ShowConnectionRejectedMessage(EP48NicknameRegistrationResult::InvalidFormat);
	}
	else if (FailureMessage.Equals(TEXT("UserAlreadyRegistered")))
	{
		UIManager->ShowConnectionRejectedMessage(EP48NicknameRegistrationResult::UserAlreadyRegistered);
	}
	else
	{
		// default
		UIManager->ShowConnectionRejectedMessage(EP48NicknameRegistrationResult::DatabaseError);
	}
}*/
