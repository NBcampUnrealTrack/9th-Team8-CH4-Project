#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../DataTable/WeaponDataTypes.h"
#include "P48WeaponBase.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UGameplayEffect;
class USoundBase;

UCLASS()
class PROJECT48_API AP48WeaponBase : public AActor
{
	GENERATED_BODY()
	
public:	
	AP48WeaponBase();
	
	virtual void OnConstruction(const FTransform& Transform) override;
	
	UFUNCTION(BlueprintPure, Category = "Weapon|Data")
	bool HasValidWeaponData() const;
	
	UFUNCTION(BlueprintPure, Category = "Weapon|Data")
	FWeaponDataRow GetWeaponData() const;
	
	UFUNCTION(BlueprintPure, Category = "Weapon|Data")
	FName GetWeaponRowName() const;
	
	UFUNCTION(BlueprintCallable, Category = "Weapon|Attack")
	void StartAttackDetection();
	
	UFUNCTION(BlueprintCallable, Category = "Weapon|Attack")
	void StopAttackDetection();
	
	UFUNCTION(BlueprintCallable, Category = "Weapon|Drop")
	void OnDropped(const FVector& DropImpulse);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDropped(const FVector& DropImpulse);

	void PlaySwingSound();
	
	void ResetAttackState();

protected:
	virtual void BeginPlay() override;
	virtual void PostNetInit() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Component")
	TObjectPtr<UStaticMeshComponent> WeaponMeshComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Component")
	TObjectPtr<UBoxComponent> AttackCollisionComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Data")
	FDataTableRowHandle WeaponDataHandle;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|GAS")
	TSubclassOf<UGameplayEffect> GroggyDamageEffectClass;
	
private:
	void ApplyWeaponData();
	
	UPROPERTY(Transient)
	FWeaponDataRow CachedWeaponData;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CachedSwingSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CachedHitSound;
	
	UPROPERTY(Transient)
	bool bHasValidWeaponData = false;
	
	UPROPERTY(Transient)
	bool bIsAttackDetectionActive = false;
	
	UFUNCTION()
	void OnAttackCollisionBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	
	bool bHasHitActorThisAttack = false;
	
	bool HandleWeaponHit(AActor* HitActor);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitSound(FVector_NetQuantize HitLocation);

	void PlayHitSoundAtLocation(const FVector& HitLocation);
	
	FVector CalculateKnockbackDirection(const AActor* HitActor) const;
};
