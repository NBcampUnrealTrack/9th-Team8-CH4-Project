#include "P48DeathFloor.h"

#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Project48/Character/P48PlayerCharacter.h"
#include "Project48/Character/P48PlayerState.h"

const FVector AP48DeathFloor::DefaultCollisionExtent(5000.0f, 5000.0f, 500.0f);

AP48DeathFloor::AP48DeathFloor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = false;

	DeathCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("DeathCollision"));
	SetRootComponent(DeathCollision);

	CollisionSize = DefaultCollisionExtent * 2.0f;
	DeathCollision->InitBoxExtent(CollisionSize * 0.5f);
	DeathCollision->SetCollisionProfileName(TEXT("Trigger"));
	DeathCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DeathCollision->SetGenerateOverlapEvents(true);
	DeathCollision->SetCanEverAffectNavigation(false);

	DeathCollision->OnComponentBeginOverlap.AddDynamic(
		this, &ThisClass::OnDeathCollisionBeginOverlap);
}

void AP48DeathFloor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (IsValid(DeathCollision))
	{
		const FVector SafeSize(
			FMath::Max(1.0f, CollisionSize.X),
			FMath::Max(1.0f, CollisionSize.Y),
			FMath::Max(1.0f, CollisionSize.Z));
		DeathCollision->SetBoxExtent(SafeSize * 0.5f);
	}
}

void AP48DeathFloor::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(DeathCollision))
	{
		return;
	}

	// 레벨에 저장된 컴포넌트 인스턴스 값이 생성자의 충돌 설정을 덮어쓸 수 있으므로
	// 플레이 직전에 서버와 클라이언트 모두 동일한 설정을 강제로 적용합니다.
	ApplyRuntimeCollisionSettings();
	DeathCollision->UpdateOverlaps();

	if (!bDrawDebugBounds)
	{
		return;
	}

	const FVector Center = DeathCollision->GetComponentLocation();
	const FVector Extent = DeathCollision->GetScaledBoxExtent();
	DrawDebugBox(
		GetWorld(),
		Center,
		Extent,
		DeathCollision->GetComponentQuat(),
		FColor::Red,
		true,
		-1.0f,
		0,
		10.0f);

	UE_LOG(LogTemp, Warning,
		TEXT("[DeathFloor Debug] %s Center=%s Size=%s Scale=%s Authority=%s Collision=%d PawnResponse=%d GenerateOverlap=%s"),
		*GetName(),
		*Center.ToCompactString(),
		*(Extent * 2.0f).ToCompactString(),
		*GetActorScale3D().ToCompactString(),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		static_cast<int32>(DeathCollision->GetCollisionEnabled()),
		static_cast<int32>(DeathCollision->GetCollisionResponseToChannel(ECC_Pawn)),
		DeathCollision->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"));
}

void AP48DeathFloor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && IsValid(DeathCollision))
	{
		KillPlayersInsideBounds();
	}
}

void AP48DeathFloor::ApplyRuntimeCollisionSettings()
{
	DeathCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DeathCollision->SetCollisionObjectType(ECC_WorldStatic);
	DeathCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	DeathCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DeathCollision->SetGenerateOverlapEvents(true);
	DeathCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AP48DeathFloor::KillPlayersInsideBounds()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(P48DeathFloorTick), false, this);

	World->OverlapMultiByObjectType(
		OverlapResults,
		DeathCollision->GetComponentLocation(),
		DeathCollision->GetComponentQuat(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(DeathCollision->GetScaledBoxExtent()),
		QueryParams);

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		if (AP48PlayerCharacter* P48Character = Cast<AP48PlayerCharacter>(OverlapResult.GetActor()))
		{
			KillPlayerOnce(P48Character, TEXT("tick query"));
		}
	}
}

void AP48DeathFloor::KillPlayerOnce(AP48PlayerCharacter* PlayerCharacter, const TCHAR* DetectionSource)
{
	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	AP48PlayerState* PlayerState = PlayerCharacter->GetPlayerState<AP48PlayerState>();
	if (!IsValid(PlayerState) || !PlayerState->IsAlive())
	{
		return;
	}

	const TWeakObjectPtr<AP48PlayerCharacter> PlayerKey(PlayerCharacter);
	if (TickDetectedPlayers.Contains(PlayerKey))
	{
		return;
	}

	TickDetectedPlayers.Add(PlayerKey);
	UE_LOG(LogTemp, Warning, TEXT("[Server] %s detected inside death floor %s by %s"),
		*GetNameSafe(PlayerCharacter), *GetName(), DetectionSource);
	PlayerCharacter->Death();
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

	KillPlayerOnce(P48Character, TEXT("overlap event"));
}
