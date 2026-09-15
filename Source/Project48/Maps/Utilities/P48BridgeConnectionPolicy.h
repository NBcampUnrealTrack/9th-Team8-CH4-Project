#pragma once

#include "CoreMinimal.h"

struct FP48BridgeConnectionRules;

/** 한 섬 쌍에서 실제 다리 끝점으로 사용할 수 있는 후보입니다. */
struct PROJECT48_API FP48BridgeEndpointCandidate
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	float Length = 0.0f;
};

namespace P48BridgeConnectionPolicy
{
	/** 섬 배치와 다리 생성이 함께 사용하는 거리, 높이, 길이, 경사 판정입니다. */
	PROJECT48_API bool IsGeometryValid(
		const FVector& Start,
		const FVector& End,
		const FP48BridgeConnectionRules& Rules,
		float& OutLength);

	/** Start/End 방향과 무관하게 두 다리가 같은 끝점을 사용하는지 검사합니다. */
	PROJECT48_API bool AreSameUndirectedEndpoints(
		const FVector& StartA,
		const FVector& EndA,
		const FVector& StartB,
		const FVector& EndB,
		float Tolerance);

	/** 높은 공통 높이, 작은 높이 차이, 짧은 길이 순으로 후보를 안정적으로 정렬합니다. */
	PROJECT48_API void SortEndpointCandidates(TArray<FP48BridgeEndpointCandidate>& Candidates);

	/** 후보 한 쌍이 두 섬의 현재 기둥 점유 반경을 모두 피하는지 검사합니다. */
	PROJECT48_API bool IsCandidateAvailable(
		const FP48BridgeEndpointCandidate& Candidate,
		int32 StartIslandIndex,
		int32 EndIslandIndex,
		const TMap<int32, TArray<FVector>>& UsedEndpointsByIsland,
		float EndpointExclusionRadius);

	/** 이미 예약된 같은 섬의 기둥 점유 반경과 겹치지 않는 첫 후보를 찾습니다. */
	PROJECT48_API int32 FindFirstAvailableCandidate(
		const TArray<FP48BridgeEndpointCandidate>& Candidates,
		int32 StartIslandIndex,
		int32 EndIslandIndex,
		const TMap<int32, TArray<FVector>>& UsedEndpointsByIsland,
		float EndpointExclusionRadius);

	/** 선택한 후보의 양 끝점을 해당 섬의 기둥 점유 위치로 예약합니다. */
	PROJECT48_API void ReserveCandidateEndpoints(
		const FP48BridgeEndpointCandidate& Candidate,
		int32 StartIslandIndex,
		int32 EndIslandIndex,
		TMap<int32, TArray<FVector>>& UsedEndpointsByIsland);

	/** 부모와 자식 섬의 월드 다리 기준 높이가 같아지는 자식 섬 기준 Z를 계산합니다. */
	PROJECT48_API bool TryCalculateAlignedIslandHeight(
		float ParentBaseHeight,
		float ParentAnchorOffset,
		float ChildAnchorOffset,
		float MinBaseHeight,
		float MaxBaseHeight,
		float& OutChildBaseHeight);
}
