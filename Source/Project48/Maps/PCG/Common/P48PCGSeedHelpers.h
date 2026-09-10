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

/** ParamData를 우선 사용하고, 연결되지 않은 노드에서는 WorldSubsystem의 현재 Context를 읽습니다. */
bool P48ReadGenerationContext(FPCGContext* Context, FP48PCGGenerationContext& OutContext);

/** SharedSeed 입력이나 생성 Context가 없으면 기존 PCG 시드를 사용합니다. */
int32 P48ReadNetworkSeed(FPCGContext* Context);
