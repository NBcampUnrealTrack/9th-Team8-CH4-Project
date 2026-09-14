#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "P48PlayerController.generated.h"

class UP48UIManagerComponent;
class UInputMappingContext;
class UUserWidget;

UCLASS()
class PROJECT48_API AP48PlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;
	virtual void AcknowledgePossession(APawn* P) override;
	
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetReady(bool bNewReady);
	
private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputMappingContext> SpectatorIMC;
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Test|UI")
	TSubclassOf<UUserWidget> WidgetClass;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Test|UI")
	TObjectPtr<UUserWidget> WidgetInstance;
	
	UFUNCTION(BlueprintCallable, Category = "Match|Ready")
	void RequestReady();

	/** 로컬 PCG가 현재 세대 생성을 마쳤음을 서버에 알립니다. */
	void ReportMapGenerationComplete(int32 GenerationId);
	
	//ClientRPC
	UFUNCTION(Client, Reliable)
	void Client_SetPlayInputBlocked(bool bBlocked);

protected:
	UFUNCTION(Server, Reliable)
	void Server_ReportMapGenerationComplete(int32 GenerationId);
	
	/* UI */
public:
	AP48PlayerController();
	
	virtual void SetupInputComponent() override;
	virtual void ClientWasKicked_Implementation(const FText& KickReason) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UP48UIManagerComponent>  UIManagerComp;
	
	void ToggleChatInput();
};
