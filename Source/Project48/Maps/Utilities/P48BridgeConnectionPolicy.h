#pragma once

#include "CoreMinimal.h"

struct FP48BridgeConnectionRules;

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
}
