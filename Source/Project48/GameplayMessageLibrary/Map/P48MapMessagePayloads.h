#pragma once

#include "CoreMinimal.h"
#include "P48MapMessagePayloads.generated.h"

/** 맵 생성을 시작할 때 필요한 입력을 전달하는 메시지입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48MapGenerationRequestMessage
{
	GENERATED_BODY()

	FP48MapGenerationRequestMessage() = default;

	FP48MapGenerationRequestMessage(const int32 InPlayerCount, const int32 InSeed)
		: PlayerCount(InPlayerCount)
		, Seed(InSeed)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Message|Map")
	int32 PlayerCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Message|Map")
	int32 Seed = 0;
};

/** 맵 생성 작업의 완료 결과를 전달하는 메시지입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48MapGenerationCompletedMessage
{
	GENERATED_BODY()

	FP48MapGenerationCompletedMessage() = default;

	FP48MapGenerationCompletedMessage(
		const int32 InGenerationId,
		const int32 InSeed,
		const bool bInSucceeded)
		: GenerationId(InGenerationId)
		, Seed(InSeed)
		, bSucceeded(bInSucceeded)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Message|Map")
	int32 GenerationId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Message|Map")
	int32 Seed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Message|Map")
	bool bSucceeded = false;
};
