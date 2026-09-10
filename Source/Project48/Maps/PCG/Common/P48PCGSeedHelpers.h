#pragma once

#include "CoreMinimal.h"
#include "P48PCGGenerationTypes.h"

struct FPCGContext;

namespace P48PCGSeedNames
{
	inline const FName InputPin(TEXT("SharedSeed"));
	inline const FName Seed(TEXT("Seed"));
	inline const FName GenerationId(TEXT("GenerationId"));
	inline const FName RequiredPlayerCount(TEXT("RequiredPlayerCount"));
}

/**
 * Seed는 PCG Component의 Seed를 공통 입력으로 사용합니다.
 * 게임 월드에서는 복제된 생성 Context로 GenerationId/PlayerCount를 보완합니다.
 */
bool P48ReadGenerationContext(FPCGContext* Context, FP48PCGGenerationContext& OutContext);

/** 서버와 클라이언트에 동일하게 주입된 PCG Component Seed를 반환합니다. */
int32 P48ReadNetworkSeed(FPCGContext* Context);
