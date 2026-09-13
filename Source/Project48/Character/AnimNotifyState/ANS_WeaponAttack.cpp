#include "ANS_WeaponAttack.h"

#include "Project48/Character/P48PlayerCharacter.h"
#include "Project48/Weapon/P48WeaponBase.h"

void UANS_WeaponAttack::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	
	if (MeshComp)
	{
		if (AP48PlayerCharacter* PlayerCharacter = Cast<AP48PlayerCharacter>(MeshComp->GetOwner()))
		{
			if (AP48WeaponBase* Weapon = Cast<AP48WeaponBase>(PlayerCharacter->GetEquippedWeapon()))
			{
				Weapon->StartAttackDetection();
			}
		}
	}
}

void UANS_WeaponAttack::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	
	if (MeshComp)
	{
		if (AP48PlayerCharacter* PlayerCharacter = Cast<AP48PlayerCharacter>(MeshComp->GetOwner()))
		{
			if (AP48WeaponBase* Weapon = Cast<AP48WeaponBase>(PlayerCharacter->GetEquippedWeapon()))
			{
				Weapon->StopAttackDetection();
			}
		}
	}
}