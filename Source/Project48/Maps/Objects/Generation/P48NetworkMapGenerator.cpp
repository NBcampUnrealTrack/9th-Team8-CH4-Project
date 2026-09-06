#include "P48NetworkMapGenerator.h"

#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "PCGComponent.h"
#include "Engine/World.h"
#include "Misc/Guid.h"

AP48NetworkMapGenerator::AP48NetworkMapGenerator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
	MapPCG = CreateDefaultSubobject<UPCGComponent>(TEXT("MapPCG"));
	MapPCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
}

void AP48NetworkMapGenerator::BeginPlay()
{
	Super::BeginPlay();
	MapPCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
	if (HasAuthority())
	{
		int32 StartSeed = InitialSeed;
		if (bRandomizeSeedOnPIEStart && GetWorld() && GetWorld()->WorldType == EWorldType::PIE)
		{
			StartSeed = static_cast<int32>(GetTypeHash(FGuid::NewGuid()) & MAX_int32);
		}
		SetMapSeed(StartSeed);
	}
	else { OnRep_GenerationState(); }
}

void AP48NetworkMapGenerator::SetMapSeed(const int32 NewSeed)
{
	if (!HasAuthority()) { return; }
	GenerationState.Seed = NewSeed;
	GenerationState.GenerationId = GenerationState.GenerationId == MAX_int32 ? 1 : GenerationState.GenerationId + 1;
	FlushNetDormancy();
	ForceNetUpdate();
	OnRep_GenerationState();
}

void AP48NetworkMapGenerator::OnRep_GenerationState()
{
	if (GenerationState.GenerationId != 0) { SetActorTickEnabled(true); }
}

void AP48NetworkMapGenerator::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// 실행 중인 그래프를 동시에 정리하지 않습니다. 대기 요청은 최신 복제 상태로 합쳐집니다.
	if (!HasActorBegunPlay() || MapPCG->IsGenerating()) { return; }
	if (GenerationState.GenerationId == 0 || StartedGenerationId == GenerationState.GenerationId)
	{
		SetActorTickEnabled(false);
		return;
	}
	StartedGenerationId = GenerationState.GenerationId;
	if (!MapPCG->GetGraph())
	{
		UE_LOG(LogTemp, Warning, TEXT("[P48MapSeed] %s: Assign a graph to MapPCG before generation."), *GetName());
		SetActorTickEnabled(false);
		return;
	}
	MapPCG->CleanupLocalImmediate(true);
	MapPCG->Seed = GenerationState.Seed;
	UE_LOG(LogTemp, Display, TEXT("[P48MapSeed] Authority=%d Seed=%d GenerationId=%d"), HasAuthority(), GenerationState.Seed, GenerationState.GenerationId);
	// Generate()는 네트워크 호출을 포함하므로 각 머신의 로컬 실행 API를 사용합니다.
	MapPCG->GenerateLocal(true);
}

void AP48NetworkMapGenerator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AP48NetworkMapGenerator, GenerationState);
}

void AP48NetworkMapGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetActorTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}
