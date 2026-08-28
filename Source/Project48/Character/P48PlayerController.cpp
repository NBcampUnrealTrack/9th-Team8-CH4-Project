#include "P48PlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

void AP48PlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			if (IsValid(DefaultMappingContext) == true)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}