#pragma once

#include "CoreMinimal.h"
#include "P48GameplayMessagePayloads.generated.h"

/** 추가 데이터 없이 사건이 발생했다는 사실만 전달할 때 사용하는 공통 Payload입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48EmptyMessage
{
	GENERATED_BODY()
};
