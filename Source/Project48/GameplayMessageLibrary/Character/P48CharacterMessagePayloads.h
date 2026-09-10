#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "P48CharacterMessagePayloads.generated.h"

class AActor;

/** 특정 캐릭터에게 사망 처리를 요청할 때 전달하는 메시지입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48CharacterDeathRequestMessage
{
	GENERATED_BODY()

	FP48CharacterDeathRequestMessage() = default;

	FP48CharacterDeathRequestMessage(
		AActor* InTargetActor,
		AActor* InSourceActor = nullptr,
		const FGameplayTag InReason = FGameplayTag())
		: TargetActor(InTargetActor)
		, SourceActor(InSourceActor)
		, Reason(InReason)
	{
	}

	/** 사망 요청을 받아야 하는 캐릭터입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Message|Character")
	TObjectPtr<AActor> TargetActor = nullptr;

	/** 사망 요청을 발생시킨 액터입니다. 낙사 영역이나 공격자 등이 될 수 있습니다. */
	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Message|Character")
	TObjectPtr<AActor> SourceActor = nullptr;

	/** 낙사, 전투 등 세부 사유가 필요할 때 사용하는 선택 태그입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "Gameplay Message|Character")
	FGameplayTag Reason;
};
