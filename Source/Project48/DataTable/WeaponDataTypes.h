#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WeaponDataTypes.generated.h"

class UStaticMesh;
class USoundBase;

USTRUCT(BlueprintType)
struct PROJECT48_API FWeaponDataRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Info")
	FText DisplayName;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Asset")
	TSoftObjectPtr<UStaticMesh> WeaponMesh;
	
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Weapon|Balance",
		meta = (ClampMin = "0.0")
		)
	float GroggyDamage = 10.0f;
	
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Weapon|Balance",
		meta = (ClampMin = "0.0")
		)
	float KnockbackPower = 500.0f;
	
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Weapon|Balance",
		meta = (ClampMin = "0.0")
		)
	float AttackCooldown = 1.0f;
		
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Weapon|Balance",
		meta = (ClampMin = "0.01")
		)
	float Weight = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Audio")
	TSoftObjectPtr<USoundBase> SwingSound;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Audio")
	TSoftObjectPtr<USoundBase> HitSound;
	
	bool HasValidBalanceData() const;
};
