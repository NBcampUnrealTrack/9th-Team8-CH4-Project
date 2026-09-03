#include "P48GE_Run.h"

UP48GE_Run::UP48GE_Run()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	
	FGameplayTag RunTag = FGameplayTag::RequestGameplayTag(FName("State.Movement.Running"), false);
	
	FInheritedTagContainer TagContainer;
	TagContainer.AddTag(RunTag);
	InheritableOwnedTagsContainer = TagContainer;
}