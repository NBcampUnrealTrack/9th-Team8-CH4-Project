#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UWorld;

/** Trace 단위 횟수와 Hit 단위 탈락 횟수는 별도로 집계합니다. */
struct FP48IslandSurfaceTraceStats
{
	int32 Traces = 0;
	int32 NoHits = 0;
	int32 Hits = 0;
	int32 MeshMismatchHits = 0;
	int32 InvalidInstanceHits = 0;
	int32 TransformMismatchHits = 0;
	int32 MatchedTraces = 0;
	FString FirstMismatch;
};

/** 특정 섬 메시 인스턴스의 충돌 표면만 검사합니다. PCG 출력과 스폰은 담당하지 않습니다. */
class FP48IslandSurfaceSampler
{
public:
	/** PlayerStart sampling requires simple collision that blocks Pawn; bridge sampling may use the visual surface. */
	static bool TraceTop(UWorld* World, UStaticMesh* Mesh, const FTransform& IslandTransform, const FBox& WorldBounds, const FVector& XY, FHitResult& OutHit, FP48IslandSurfaceTraceStats* Stats = nullptr, bool bRequirePawnSupport = false);
};
