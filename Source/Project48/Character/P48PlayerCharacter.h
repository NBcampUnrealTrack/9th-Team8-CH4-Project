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
class AP48WeaponBase;
struct FInputActionValue;

UCLASS()
class PROJECT48_API AP48PlayerCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AP48PlayerCharacter();
	
	virtual void Destroyed() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
protected:
	virtual void BeginPlay() override;
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
	
	//UI
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Nickname")
	TObjectPtr<UP48PlayerNameWidgetComponent> NicknameWidgetComponent;
	
	//Weapon
	UPROPERTY(ReplicatedUsing = OnRep_Weapon)
	TObjectPtr<AP48WeaponBase> Weapon;
	
	UFUNCTION()
	void OnRep_Weapon();

	//Carry
	UPROPERTY(ReplicatedUsing = OnRep_CarriedCharacter)
	TObjectPtr<AP48PlayerCharacter> CarriedCharacter;

	UPROPERTY(ReplicatedUsing = OnRep_CarrierCharacter)
	TObjectPtr<AP48PlayerCharacter> CarrierCharacter;

	UFUNCTION()
	void OnRep_CarriedCharacter();

	UFUNCTION()
	void OnRep_CarrierCharacter();

	//Input Block
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Input")
	bool bInputBlocked = false;

public:	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	//GAS
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	//attack/hit
	UFUNCTION(BlueprintCallable, Category="Attack|Attack")
	void StartPunchAttack();
	
	UFUNCTION(BlueprintCallable, Category="Attack|Attack")
	void StopPunchAttack();
	
	UFUNCTION(BlueprintCallable, Category="Attack|Hit")
	void OnHit(const FVector& HitLocation, const FVector& HitDirection, float ImpulseStrength);
	
	//Input Block
	void SetInputBlocked(bool bBlocked);
	FORCEINLINE bool IsInputBlocked() const { return bInputBlocked; }
	
	//Death
	UFUNCTION(BlueprintCallable, Category="Death|Death")
	void Death();
	
	//Weapon
	FORCEINLINE class AP48WeaponBase* GetEquippedWeapon() const { return Weapon; }
	
	//Carry / Throw
	FORCEINLINE bool IsCarryingCharacter() const { return CarriedCharacter != nullptr; }
	FORCEINLINE bool IsBeingCarried() const { return CarrierCharacter != nullptr; }
	FORCEINLINE AP48PlayerCharacter* GetCarriedCharacter() const { return CarriedCharacter; }
	FORCEINLINE AP48PlayerCharacter* GetCarrierCharacter() const { return CarrierCharacter; }
	bool IsAlive() const;

	void OnPickedUpBy(AP48PlayerCharacter* InCarrier);
	void OnThrown(const FVector& ReleaseLocation, const FRotator& ReleaseRotation, const FVector& ThrowVelocity, AP48PlayerCharacter* InCarrier);
	void OnDroppedFromCarrier(const FVector& DropLocation, const FRotator& DropRotation, AP48PlayerCharacter* InCarrier);
	
	
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
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInputAction> IA_PickUp;
	
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
	
	UFUNCTION(Server, Reliable)
	void Server_EquipWeapon(AP48WeaponBase* NewWeapon);
	
	UFUNCTION(Server, Reliable)
	void Server_DropWeapon();

	UFUNCTION(Server, Reliable)
	void Server_PickUpCharacter(AP48PlayerCharacter* TargetCharacter);

	UFUNCTION(Server, Reliable)
	void Server_ThrowCharacter();

	UFUNCTION(Server, Reliable)
	void Server_DropCharacter();
	
	//MulticastRPC
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_OnHit(const FVector& HitLocation, const FVector& Impulse);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayPunchMontage();
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayWeaponMontage();
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_DeathRagDoll();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayThrowMontage();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnThrown(const FVector& ReleaseLocation, const FRotator& ReleaseRotation, const FVector& ThrowVelocity, AP48PlayerCharacter* InCarrier);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDropped(const FVector& DropLocation, const FRotator& DropRotation, AP48PlayerCharacter* InCarrier);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_RecoverFromRagdoll(const FVector& StandLocation, const FRotator& StandRotation);

	void Server_RecoverFromRagdoll();

	FName GetRagdollRootBoneName() const;

	//Carry Settings
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry", meta=(AllowPrivateAccess="true"))
	FName CarrySocketName = TEXT("handslot_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry", meta=(AllowPrivateAccess="true"))
	float ThrowForwardSpeed = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry", meta=(AllowPrivateAccess="true"))
	float ThrowUpSpeed = 600.0f;

	// 던져진 후 바닥에 누워있는 래그돌 지속 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry", meta=(AllowPrivateAccess="true"))
	float RagdollRecoverDuration = 1.5f;

	// 래그돌에서 일어날 때 재생할 몽타주 (선택 사항)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> GetUpMontage;

	// 캐리어 기준 전방 오프셋 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry", meta=(AllowPrivateAccess="true"))
	float CarriedForwardOffset = 15.0f;

	// 캐리어 기준 우측 오프셋 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry", meta=(AllowPrivateAccess="true"))
	float CarriedRightOffset = 0.0f;

	// 캐리어 손 기준 높이 오프셋 (cm, 음수면 손 아래로 캡슐 중심이 위치하여 멱살/목덜미를 잡는 모양)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry", meta=(AllowPrivateAccess="true"))
	float CarriedHeightOffset = -40.0f;

	// 캐리어 기준 Yaw 회전 오프셋 (0이면 정면, 180이면 캐리어를 마주봄)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry", meta=(AllowPrivateAccess="true"))
	float CarriedYawOffset = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAnimMontage> ThrowMontage;

	// 들려있는 동안 래그돌 물리 시뮬레이션 활성화 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry|Physics", meta=(AllowPrivateAccess="true"))
	bool bEnableCarriedRagdoll = true;

	// 들려있는 동안 물리 블렌딩 비율 (0.0 = 완전 뻣뻣함, 1.0 = 완전 래그돌, 0.5~0.6 = 자연스럽게 덜렁거림)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry|Physics", meta=(AllowPrivateAccess="true", ClampMin="0.0", ClampMax="1.0"))
	float CarriedPhysicsBlendWeight = 0.6f;

	// 래그돌을 시뮬레이션할 기준 본 이름 (pelvis는 루트이므로 이탈 방지를 위해 spine 권장)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry|Physics", meta=(AllowPrivateAccess="true"))
	FName CarriedPhysicsBoneName = TEXT("spine");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry|Debug", meta=(AllowPrivateAccess="true"))
	bool bEnableCarryDebug = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Carry", meta=(AllowPrivateAccess="true"))
	bool bIsInThrownRagdoll = false;

	FTimerHandle RagdollRecoverTimerHandle;
	
	//attack/hit
	UPROPERTY()
	TSet<TWeakObjectPtr<AActor>> HitActorThisPunch;
	
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Run();
	void StopRun();
	void Attack();
	
	void AttackHandle();
	
	//weapon
	void EquipWeaponHandle();
	
public:
	virtual void Jump() override;
	virtual void OnJumped_Implementation() override;
	virtual void Landed(const FHitResult& Hit) override;
};
