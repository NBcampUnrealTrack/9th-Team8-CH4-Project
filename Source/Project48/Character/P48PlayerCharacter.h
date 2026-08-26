#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "P48PlayerCharacter.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS()
class PROJECT48_API AP48PlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AP48PlayerCharacter();

protected:
	virtual void BeginPlay() override;
	
public:	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputMappingContext> InputMappingContext;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> IA_Move;
		
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> IA_Look;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> IA_Jump;
};
