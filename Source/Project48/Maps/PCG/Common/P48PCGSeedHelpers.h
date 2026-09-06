#pragma once

#include "CoreMinimal.h"

struct FPCGContext;

namespace P48PCGSeedNames
{
	inline const FName InputPin(TEXT("SharedSeed"));
	inline const FName Seed(TEXT("Seed"));
}

/** SharedSeed 입력이 없으면 기존 PCG 시드를 사용합니다. */
int32 P48ReadNetworkSeed(FPCGContext* Context);
