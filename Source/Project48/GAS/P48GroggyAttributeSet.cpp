#include "P48GroggyAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UP48GroggyAttributeSet::UP48GroggyAttributeSet()
{
	InitGroggy(0.0f);
	InitMaxGroggy(100.0f);
}

void UP48GroggyAttributeSet::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(
		UP48GroggyAttributeSet,
		Groggy,
		COND_None,
		REPNOTIFY_Always);
	
	DOREPLIFETIME_CONDITION_NOTIFY(
		UP48GroggyAttributeSet,
		MaxGroggy,
		COND_None,
		REPNOTIFY_Always);
}

void UP48GroggyAttributeSet::PreAttributeChange(
	const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	if (Attribute == GetMaxGroggyAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
}

void UP48GroggyAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
	const FGameplayAttribute& ModifiedAttribute =
		Data.EvaluatedData.Attribute;
	
	if (ModifiedAttribute == GetMaxGroggyAttribute())
	{
		SetMaxGroggy(FMath::Max(GetMaxGroggy(), 1.0f));
	}
	
	if (ModifiedAttribute == GetGroggyAttribute() ||
		ModifiedAttribute == GetMaxGroggyAttribute())
	{
		SetGroggy(FMath::Clamp(GetGroggy(), 0.0f, GetMaxGroggy()));
	}
}

void UP48GroggyAttributeSet::OnRep_Groggy(const FGameplayAttributeData& OldGroggy)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UP48GroggyAttributeSet, Groggy, OldGroggy);
}

void UP48GroggyAttributeSet::OnRep_MaxGroggy(const FGameplayAttributeData& OldMaxGroggy)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UP48GroggyAttributeSet, MaxGroggy, OldMaxGroggy);
}
