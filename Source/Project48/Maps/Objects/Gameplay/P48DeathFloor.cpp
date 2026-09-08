#include "P48DeathFloor.h"

#include "Components/BoxComponent.h"
#include "Project48/Character/P48PlayerCharacter.h"

AP48DeathFloor::AP48DeathFloor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	DeathCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("DeathCollision"));
	SetRootComponent(DeathCollision);

	// PCG 포인트의 Scale이 최종 월드 크기를 결정하도록 기본 전체 크기를 100cm로 둡니다.
	DeathCollision->InitBoxExtent(FVector(50.0f));
	DeathCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DeathCollision->SetCollisionObjectType(ECC_WorldStatic);
	DeathCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	DeathCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DeathCollision->SetGenerateOverlapEvents(true);
	DeathCollision->SetCanEverAffectNavigation(false);

	DeathCollision->OnComponentBeginOverlap.AddDynamic(
		this, &ThisClass::OnDeathCollisionBeginOverlap);
}

void AP48DeathFloor::OnDeathCollisionBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	AP48PlayerCharacter* P48Character = Cast<AP48PlayerCharacter>(OtherActor);
	if (!IsValid(P48Character))
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Server] %s entered death floor %s"),
		*GetNameSafe(P48Character), *GetName());

	// TODO: 캐릭터 사망 처리가 준비되면 직접 호출 대신 Gameplay Message 전송으로 교체합니다.
	// P48Character->Death();
}
