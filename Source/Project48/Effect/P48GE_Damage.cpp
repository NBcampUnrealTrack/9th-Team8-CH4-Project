#include "P48GE_Damage.h"

#include "Project48/GAS/P48GroggyAttributeSet.h"

UP48GE_Damage::UP48GE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	
	FGameplayModifierInfo ModifierInfo;
	ModifierInfo.Attribute = UP48GroggyAttributeSet::GetGroggyAttribute();
	ModifierInfo.ModifierOp = EGameplayModOp::Additive;
	ModifierInfo.ModifierMagnitude = FScalableFloat(25.0f);
	
	Modifiers.Add(ModifierInfo);
}