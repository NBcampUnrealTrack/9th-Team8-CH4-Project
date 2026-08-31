#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "P48PlayerCharacter.generated.h"

class UInputAction;
class USpringArmComponent;
class UCameraComponent;
class UPhysicalAnimationComponent;
class USphereComponent;
struct FInputActionValue;

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta=(AllowPrivateAccess="true"))
	USpringArmComponent* SpringArm;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Camera", meta=(AllowPrivateAccess="true"))
	UCameraComponent* Camera;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> IA_Move;
		
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> IA_Look;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> IA_Jump;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> IA_Run;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> IA_Attack;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USoundBase> JumpSound;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Run", meta=(AllowPrivateAccess="true"))
	float WalkSpeedMultiplier;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Walk", meta=(AllowPrivateAccess="true"))
	float DefaultWalkSpeed;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Run", meta=(AllowPrivateAccess="true"))
	bool bIsSprint;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack", meta=(AllowPrivateAccess="true"))
	bool bEquipWeapon;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Collision", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> RightHandHitbox;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Collision", meta=(AllowPrivateAccess="true"))
	FVector RightHandHitboxOffset;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Collision", meta=(AllowPrivateAccess="true"))
	float RightHandHitboxRadius;
	
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Run();
	void StopRun();
	void Attack();
	
public:
	virtual void Jump() override;
	virtual void OnJumped_Implementation() override;
};
