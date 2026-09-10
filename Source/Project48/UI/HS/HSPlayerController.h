#pragma once

#include "CoreMinimal.h"
#include "Project48/Character/P48PlayerController.h"
#include "HSPlayerController.generated.h"

class UP48UIManagerComponent;

UCLASS()
class PROJECT48_API AHSPlayerController : public AP48PlayerController
{
	GENERATED_BODY()
	
public:
	AHSPlayerController();
	
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UP48UIManagerComponent>  UIManagerComp;
	
	void ToggleChatInput();
};
