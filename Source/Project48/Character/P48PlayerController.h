#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "P48PlayerController.generated.h"

class UInputMappingContext;

UCLASS()
class PROJECT48_API AP48PlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;
	
private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
};
