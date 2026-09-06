#include "ANS_PunchAttack.h"

#include "Project48/Character/P48PlayerCharacter.h"

void UANS_PunchAttack::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	
	if (MeshComp)
	{
		if (AP48PlayerCharacter* Character = Cast<AP48PlayerCharacter>(MeshComp->GetOwner()))
		{
			Character->StartPunchAttack();
		}
	}
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Punch Start"));
	}
}

void UANS_PunchAttack::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	
	if (MeshComp)
	{
		if (AP48PlayerCharacter* Character = Cast<AP48PlayerCharacter>(MeshComp->GetOwner()))
		{
			Character->StopPunchAttack();
		}
	}
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Punch End"));
	}
}