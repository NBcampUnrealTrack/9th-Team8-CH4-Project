#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_SetPhysicsBlendWeight.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT48_API UANS_SetPhysicsBlendWeight : public UAnimNotifyState
{
	GENERATED_BODY()
	
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float AttackPhysicsWeight = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float DefaultPhysicsWeight = 0.8f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float InterpSpeed = 15.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FName TargetBoneName = FName("upperarm_r");
	
private:
	float CurrentBlendWeight = 1.0f;
};
