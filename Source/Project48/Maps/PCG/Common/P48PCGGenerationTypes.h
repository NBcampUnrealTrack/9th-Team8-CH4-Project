#pragma once

#include "CoreMinimal.h"
#include "P48PCGGenerationTypes.generated.h"

UENUM()
enum class EP48PCGGenerationPhase : uint8
{
	Idle,
	Cleaning,
	Generating,
	WaitingForPlayerCount,
	WaitingForPlayerStarts,
	WaitingForClients,
	Ready,
	Failed
};

/** 한 PCG 생성 Revision의 네트워크 입력과 진행 상태입니다. */
USTRUCT()
struct FP48PCGGenerationSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Seed = 0;

	UPROPERTY()
	int32 Revision = 0;

	UPROPERTY()
	int32 RequiredPlayerCount = 0;

	UPROPERTY()
	EP48PCGGenerationPhase Phase = EP48PCGGenerationPhase::Idle;

	bool HasValidSeed() const { return Revision > 0 && Seed != 0; }
	bool HasConfirmedPlayerCount() const { return RequiredPlayerCount > 0; }
	bool IsReady() const { return Phase == EP48PCGGenerationPhase::Ready; }
};

/** PCG 노드가 Subsystem/ParamData 중 한 곳에서 읽는 생성 Context입니다. */
struct FP48PCGGenerationContext
{
	int32 Seed = 0;
	int32 GenerationId = 0;
	int32 RequiredPlayerCount = 0;

	bool IsValid() const { return GenerationId > 0 && Seed != 0; }
};
