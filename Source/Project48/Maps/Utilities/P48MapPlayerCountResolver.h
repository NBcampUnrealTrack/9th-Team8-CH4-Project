#pragma once

#include "CoreMinimal.h"

class UWorld;

/** GameMode/GameState가 이미 관리하는 플레이어 수를 복사하지 않고 조회합니다. */
class PROJECT48_API FP48MapPlayerCountResolver
{
public:
	static int32 Resolve(const UWorld* World);
};
