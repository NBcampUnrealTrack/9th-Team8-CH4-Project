#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "P48GroggyAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class PROJECT48_API UP48GroggyAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	UP48GroggyAttributeSet();
	
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	virtual void PreAttributeChange(
		const FGameplayAttribute& Attribute, float& NewValue) override;
	
	virtual void PostGameplayEffectExecute(
		const FGameplayEffectModCallbackData& Data) override;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Groggy, Category = "GAS|Groggy")
	FGameplayAttributeData Groggy;
	
	ATTRIBUTE_ACCESSORS(UP48GroggyAttributeSet, Groggy)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxGroggy, Category = "GAS|Groggy")
	FGameplayAttributeData MaxGroggy;
	
	ATTRIBUTE_ACCESSORS(UP48GroggyAttributeSet, MaxGroggy)
	
protected:
	UFUNCTION()
	void OnRep_Groggy(const FGameplayAttributeData& OldGroggy);
	
	UFUNCTION()
	void OnRep_MaxGroggy(const FGameplayAttributeData& OldMaxGroggy);
};
