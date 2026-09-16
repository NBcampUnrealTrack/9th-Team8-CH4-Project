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

	/** 게임 서버 도착 확인을 받아 로비에서 시작한 재접속 시도를 종료합니다. */
	UFUNCTION(Client, Reliable)
	void Client_ConfirmGameServerArrival(bool bJoinedAsSpectator);

	UFUNCTION(Client, Reliable)
	void Client_ReturnToLobbyForMapGenerationFailure();

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
