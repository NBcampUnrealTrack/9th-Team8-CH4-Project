#include "P48IslandSurfaceSampler.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"

bool FP48IslandSurfaceSampler::TraceTop(UWorld* World, UStaticMesh* Mesh, const FTransform& IslandTransform, const FBox& WorldBounds, const FVector& XY, FHitResult& OutHit, FP48IslandSurfaceTraceStats* Stats, const bool bRequirePawnSupport)
{
	if (!World || !Mesh) { return false; }
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(P48IslandSurface), !bRequirePawnSupport);
	TArray<FHitResult> Hits;
	World->LineTraceMultiByObjectType(Hits, FVector(XY.X, XY.Y, WorldBounds.Max.Z + 10.0), FVector(XY.X, XY.Y, WorldBounds.Min.Z - 10.0), Objects, Query);
	if (Stats)
	{
		++Stats->Traces;
		Stats->Hits += Hits.Num();
		if (Hits.IsEmpty()) { ++Stats->NoHits; }
	}
	for (const FHitResult& Hit : Hits)
	{
		const UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Hit.GetComponent());
		if (!Component || Component->GetStaticMesh() != Mesh)
		{
			if (Stats) { ++Stats->MeshMismatchHits; }
			continue;
		}
		if (bRequirePawnSupport && Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
		{
			continue;
		}
		FTransform HitTransform = Component->GetComponentTransform();
		if (const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
		{
			if (!Instances->GetInstanceTransform(Hit.Item, HitTransform, true))
			{
				if (Stats) { ++Stats->InvalidInstanceHits; }
				continue;
			}
		}
		// 같은 메시를 사용하는 다른 섬과 위아래로 겹친 섬을 구분합니다.
		if (!HitTransform.Equals(IslandTransform, 0.01))
		{
			if (Stats)
			{
				++Stats->TransformMismatchHits;
				if (Stats->FirstMismatch.IsEmpty())
				{
					Stats->FirstMismatch = FString::Printf(TEXT("Component=%s Item=%d Expected={%s} Actual={%s}"), *Component->GetPathName(), Hit.Item, *IslandTransform.ToHumanReadableString(), *HitTransform.ToHumanReadableString());
				}
			}
			continue;
		}
		if (Stats) { ++Stats->MatchedTraces; }
		OutHit = Hit;
		return true;
	}
	return false;
}
