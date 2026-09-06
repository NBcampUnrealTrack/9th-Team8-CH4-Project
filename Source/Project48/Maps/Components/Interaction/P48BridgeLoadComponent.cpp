#include "P48BridgeLoadComponent.h"

#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UP48BridgeLoadComponent::UP48BridgeLoadComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UP48BridgeLoadComponent::CalculateStandingLoads(const TArray<TObjectPtr<UBoxComponent>>& Planks, TArray<FP48BridgeLoadValue>& OutLoads) const
{
	OutLoads.Reset();
	const UWorld* World = GetWorld();
	if (!World || Planks.IsEmpty())
	{
		return;
	}
	FBox BridgeBounds(ForceInit);
	for (const UBoxComponent* Plank : Planks)
	{
		if (IsValid(Plank))
		{
			BridgeBounds += Plank->Bounds.GetBox();
		}
	}
	if (!BridgeBounds.IsValid)
	{
		return;
	}
	FVector QueryExtent = BridgeBounds.GetExtent();
	QueryExtent.Z += 100.0f;
	const FVector QueryCenter = BridgeBounds.GetCenter() + FVector::UpVector * 50.0f;
	const FCollisionObjectQueryParams PawnQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(P48BridgeStandingLoad), false, GetOwner());
	TArray<FOverlapResult> Overlaps;
	if (!World->OverlapMultiByObjectType(Overlaps, QueryCenter, FQuat::Identity, PawnQuery, FCollisionShape::MakeBox(QueryExtent), QueryParams))
	{
		return;
	}
	TSet<const AActor*> ProcessedActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (const AActor* Actor = Overlap.GetActor(); Actor && !ProcessedActors.Contains(Actor))
		{
			ProcessedActors.Add(Actor);
			FP48BridgeLoadValue& Load = OutLoads.Emplace_GetRef();
			Load.WorldLocation = Actor->GetActorLocation();
			Load.Acceleration = FVector(0.0f, 0.0f, -5400.0f);
		}
	}
}
