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
	FString Nickname = "";

	UPROPERTY()
	int32 Score = 0;

	UPROPERTY()
	bool bIsMe = false;
};