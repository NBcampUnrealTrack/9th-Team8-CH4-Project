#include "P48PCGSeedWorldSubsystem.h"
#include "P48PCGNetworkSeedSettings.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"

void UP48PCGSeedWorldSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	World.GetTimerManager().SetTimerForNextTick(this, &UP48PCGSeedWorldSubsystem::CleanupGraphs);
}

void UP48PCGSeedWorldSubsystem::CleanupGraphs()
{
	PendingComponents.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TArray<UPCGComponent*> Components;
		It->GetComponents(Components);
		for (UPCGComponent* Component : Components)
		{
			const UPCGGraph* Graph = Component->GetGraph();
			if (!Graph) { continue; }
			for (const UPCGNode* Node : Graph->GetNodes())
			{
				if (Node && Cast<UP48PCGNetworkSeedSettings>(Node->GetSettings()))
				{
					// PIE에 복사된 에디터 결과와 이전 GenerationId의 액터를 먼저 제거합니다.
					// 제거와 생성을 같은 프레임에 하면 이전 충돌이 Surface Trace에 남을 수 있습니다.
					Component->CleanupLocalImmediate(true, true);
					PendingComponents.AddUnique(Component);
					break;
				}
			}
		}
	}

	if (!PendingComponents.IsEmpty())
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UP48PCGSeedWorldSubsystem::GenerateGraphs);
	}
}

void UP48PCGSeedWorldSubsystem::GenerateGraphs()
{
	for (const TWeakObjectPtr<UPCGComponent>& WeakComponent : PendingComponents)
	{
		if (UPCGComponent* Component = WeakComponent.Get())
		{
			Component->GenerateLocal(true);
		}
	}

	PendingComponents.Reset();
}
