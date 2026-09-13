#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../DataTable/WeaponDataTypes.h"
#include "P48WeaponBase.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UGameplayEffect;

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
	
	void ResetAttackState();

protected:
	virtual void BeginPlay() override;
	
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
	
	TSet<TWeakObjectPtr<AActor>> HitActorsThisAttack;
	
	void HandleWeaponHit(AActor* HitActor);
	
	FVector CalculateKnockbackDirection(const AActor* HitActor) const;
};
