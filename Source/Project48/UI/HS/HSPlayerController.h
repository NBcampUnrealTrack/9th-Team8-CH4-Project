#pragma once

#include "CoreMinimal.h"
#include "Project48/Character/P48PlayerController.h"
#include "HSPlayerController.generated.h"

class UP48UIManagerComponent;

UCLASS()
class PROJECT48_API AHSPlayerController : public AP48PlayerController
{
	GENERATED_BODY()
	
/*public:
	AHSPlayerController();
	
	virtual void SetupInputComponent() override;
	virtual void ClientWasKicked_Implementation(const FText& KickReason) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UP48UIManagerComponent>  UIManagerComp;
	
	void ToggleChatInput();*/
};
