#pragma once

#include "CoreMinimal.h"
#include "P48RankingData.generated.h"

USTRUCT(BlueprintType)
struct FP48RankingData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Rank = 0;

	UPROPERTY()
	FString Nickname = TEXT("");

	UPROPERTY()
	int32 WinCount = 0;

	UPROPERTY()
	bool bIsMe = false;
};