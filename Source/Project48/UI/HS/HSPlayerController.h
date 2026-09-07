#pragma once

#include "CoreMinimal.h"
#include "Project48/Character/P48PlayerController.h"
#include "HSPlayerController.generated.h"

UCLASS()
class PROJECT48_API AHSPlayerController : public AP48PlayerController
{
	GENERATED_BODY()
	
public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

protected:
	void ToggleChatInput();
};
