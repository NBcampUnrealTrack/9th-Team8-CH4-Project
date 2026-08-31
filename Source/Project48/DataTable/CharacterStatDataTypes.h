#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CharacterStatDataTypes.generated.h"

USTRUCT(BlueprintType)
struct PROJECT48_API FCharacterStatRow : public FTableRowBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Info")
	FText DisplayName;
	
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Character|Movement",
		meta = (ClampMin = "0.0")
		)
	float WalkSpeed = 450.0f;
	
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Character|Movement",
		meta = (ClampMin = "0.0")
		)
	float RunSpeedMultiplier = 1.5f;
	
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Character|Groggy",
		meta = (ClampMin = "1.0")
		)
	float MaxGroggy = 100.0f;
		
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Character|Groggy",
		meta = (ClampMin = "0.0")
		)
	float StunDuration = 2.0f;
};
