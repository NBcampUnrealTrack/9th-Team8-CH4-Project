#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Project48/DataTable/CharacterStatDataTypes.h"
#include "ActiveGameplayEffectHandle.h"
#include "P48PlayerCharacter.generated.h"

class UInputAction;
class USpringArmComponent;
class UCameraComponent;
class UPhysicalAnimationComponent;
class USphereComponent;
class UAbilitySystemComponent;
class UGameplayEffect;
class UP48GroggyAttributeSet;
class UP48PlayerNameWidgetComponent;
struct FInputActionValue;

UCLASS()
class PROJECT48_API AP48PlayerCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AP48PlayerCharacter();

protected:
	virtual void BeginPlay() override;

	//GAS
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	
	//DT
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stat")
	TObjectPtr<UDataTable> CharacterStatTable;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stat")
	FName CharacterStatRowName = TEXT("Player_Default");
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stat")
	FCharacterStatRow CurrentStatRow;
	
	UPROPERTY(EditDefaultsOnly, Category="GAS|Run")
	TSubclassOf<UGameplayEffect> RunEffectClass;
	
	FActiveGameplayEffectHandle RunEffectHandle;
	
	void InitializeStatsFromDataTable();
	
	//attack/hit
	UFUNCTION(BlueprintCallable, Category="Attack|Attack")
	void OnRightHandOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
	UFUNCTION(BlueprintCallable, Category="Attack|Hit")
	void OnHit(const FVector& HitLocation, const FVector& HitDirection, float ImpulseStrength);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|GAS")
	TSubclassOf<class UGameplayEffect> GroggyEffectClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|GAS")
	float GroggyDamage = 20.0f;
	
	void ApplyGroggyDamage(AActor* HitActor);
	
	//Stun
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stun|GAS")
	TSubclassOf<class UGameplayEffect> StunEffectClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stun|GAS")
	TSubclassOf<class UGameplayEffect> ResetGroggyEffectClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stun|GAS")
	TObjectPtr<UAnimMontage> StunMontage;
	
	UPROPERTY()
	FVector LastHitDirection = FVector::ZeroVector;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stun|Count")
	int32 MaxStunCount = 3;
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayStunMontage(bool bPlay, FRotator TargetRotation = FRotator::ZeroRotator);
	
	void OnGroggyChanged(const struct FOnAttributeChangeData& Data);
	
	void OnStunTagChanged(const struct FGameplayTag CallbackTag, int32 NewCount);
	
	//Death
	
	
	//UI
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Nickname")
	TObjectPtr<UP48PlayerNameWidgetComponent> NicknameWidgetComponent;
	
	void UpdateNickname();
	
public:	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	//GAS
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	//attack/hit
	UFUNCTION(BlueprintCallable, Category="Attack|Attack")
	void StartPunchAttack();
	
	UFUNCTION(BlueprintCallable, Category="Attack|Attack")
	void StopPunchAttack();

	
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
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Collision", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> PunchAttackMontage;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Collision", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> WeaponAttackMontage;
	
	//GAS
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UP48GroggyAttributeSet> GroggyAttributeSet;
	
	//ServerRPC
	UFUNCTION(Server, Reliable)
	void Server_SetMaxWalkSpeed(float NewSpeed);
	
	UFUNCTION(Server, Unreliable)
	void Server_Attack();
	
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ApplyHit(AActor* HitActor, const FVector& HitLoc, const FVector& HitDir);
	
	//MulticastRPC
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_OnHit(const FVector& HitLocation, const FVector& Impulse);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayPunchMontage();
	
	//attack/hit
	UPROPERTY()
	TSet<TWeakObjectPtr<AActor>> HitActorThisPunch;
	
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Run();
	void StopRun();
	void Attack();
	
	void AttackHandle();
public:
	virtual void Jump() override;
	virtual void OnJumped_Implementation() override;
};
