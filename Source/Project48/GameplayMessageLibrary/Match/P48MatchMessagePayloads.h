#pragma once

#include "CoreMinimal.h"
#include "P48MatchMessagePayloads.generated.h"

USTRUCT(BlueprintType)
struct PROJECT48_API FP48MatchPlayerCountMessage
{
	GENERATED_BODY()

	FP48MatchPlayerCountMessage() = default;

	explicit FP48MatchPlayerCountMessage(const int32 InPlayerCount)
		: PlayerCount(InPlayerCount)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Message|Match")
	int32 PlayerCount = 0;
};
