#pragma once

#include "CoreMinimal.h"
#include "P48WeaponMessagePayloads.generated.h"

class AActor;

/** 무기 적중 결과를 알릴 때 전달하는 메시지입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48WeaponHitMessage
{
	GENERATED_BODY()

	FP48WeaponHitMessage() = default;

	FP48WeaponHitMessage(
		AActor* InAttacker,
		AActor* InTargetActor,
		const float InGroggyDamage,
		const float InKnockbackPower,
		const FVector& InHitLocation)
		: Attacker(InAttacker)
		, TargetActor(InTargetActor)
		, GroggyDamage(InGroggyDamage)
		, KnockbackPower(InKnockbackPower)
		, HitLocation(InHitLocation)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Message|Weapon")
	TObjectPtr<AActor> Attacker = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Message|Weapon")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Message|Weapon")
	float GroggyDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Message|Weapon")
	float KnockbackPower = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Message|Weapon")
	FVector HitLocation = FVector::ZeroVector;
};
