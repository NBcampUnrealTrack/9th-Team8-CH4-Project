#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "P48PlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

UCLASS()
class PROJECT48_API AP48PlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;
	
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetReady(bool bNewReady);
	
private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Test|UI")
	TSubclassOf<UUserWidget> WidgetClass;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Test|UI")
	TObjectPtr<UUserWidget> WidgetInstance;
	
	UFUNCTION(BlueprintCallable, Category = "Match|Ready")
	void RequestReady();
};
