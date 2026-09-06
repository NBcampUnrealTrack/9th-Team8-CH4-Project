#pragma once

#include "CoreMinimal.h"
#include "ChatMessageData.generated.h"

USTRUCT(BlueprintType)
struct FChatMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString PlayerName;

	UPROPERTY()
	FString Message;
};