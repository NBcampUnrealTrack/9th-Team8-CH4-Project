#include "ANS_SetPhysicsBlendWeight.h"

void UANS_SetPhysicsBlendWeight::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	
	CurrentBlendWeight = DefaultPhysicsWeight;
	
}

void UANS_SetPhysicsBlendWeight::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	
	if (MeshComp)
	{
		if (!TargetBoneName.IsNone())
		{
			CurrentBlendWeight = FMath::FInterpTo(CurrentBlendWeight, AttackPhysicsWeight, FrameDeltaTime, InterpSpeed);
			MeshComp->SetAllBodiesBelowPhysicsBlendWeight(TargetBoneName, CurrentBlendWeight, false, true);
		}
	}
}

void UANS_SetPhysicsBlendWeight::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	
	if (MeshComp)
	{
		if (!TargetBoneName.IsNone())
		{
			MeshComp->SetAllBodiesBelowPhysicsBlendWeight(TargetBoneName, DefaultPhysicsWeight, false, true);
		}
	}
}