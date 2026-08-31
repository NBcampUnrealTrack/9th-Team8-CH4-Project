#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../DataTable/WeaponDataTypes.h"
#include "P48WeaponBase.generated.h"

class UStaticMeshComponent;

UCLASS()
class PROJECT48_API AP48WeaponBase : public AActor
{
	GENERATED_BODY()
	
public:	
	AP48WeaponBase();
	
	virtual void OnConstruction(const FTransform& Transform) override;
	
	UFUNCTION(BlueprintPure, Category = "Weapon|Data")
	bool HasvalidWeaponData() const;
	
	UFUNCTION(BlueprintPure, Category = "Weapon|Data")
	FWeaponDataRow GetWeaponData() const;
	
	UFUNCTION(BlueprintPure, Category = "Weapon|Data")
	FName GetWeaponRowName() const;

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Component")
	TObjectPtr<UStaticMeshComponent> WeaponMeshComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Data")
	FDataTableRowHandle WeaponDataHandle;
	
private:
	void ApplyWeaponData();
	
	UPROPERTY(Transient)
	FWeaponDataRow CachedWeaponData;
	
	UPROPERTY(Transient)
	bool bHasValidWeaponData = false;
};
