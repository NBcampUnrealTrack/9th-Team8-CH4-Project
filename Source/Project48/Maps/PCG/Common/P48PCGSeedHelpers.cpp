#include "P48PCGSeedHelpers.h"

#include "P48PCGSeedWorldSubsystem.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Engine/World.h"

bool P48ReadGenerationContext(FPCGContext* Context, FP48PCGGenerationContext& OutContext)
{
	if (!Context)
	{
		return false;
	}

	// 에디터 미리보기와 연결되지 않은 노드는 PCG Component Seed를 기본값으로 사용합니다.
	if (Context->ExecutionSource.IsValid())
	{
		OutContext.Seed = Context->ExecutionSource->GetExecutionState().GetSeed();
	}

	// SharedSeed가 연결되어 있으면 네트워크 Seed 노드의 값을 우선합니다.
	bool bFoundParamData = false;
	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(P48PCGSeedNames::InputPin))
	{
		const UPCGParamData* Data = Cast<UPCGParamData>(Input.Data);
		if (!Data || !Data->Metadata)
		{
			continue;
		}
		if (const auto* Attribute = Data->Metadata->GetConstTypedAttribute<int32>(P48PCGSeedNames::Seed))
		{
			OutContext.Seed = Attribute->GetValueFromItemKey(0);
			bFoundParamData = true;
		}
		if (const auto* Attribute = Data->Metadata->GetConstTypedAttribute<int32>(P48PCGSeedNames::GenerationId))
		{
			OutContext.GenerationId = Attribute->GetValueFromItemKey(0);
		}
		if (const auto* Attribute = Data->Metadata->GetConstTypedAttribute<int32>(P48PCGSeedNames::RequiredPlayerCount))
		{
			OutContext.RequiredPlayerCount = Attribute->GetValueFromItemKey(0);
		}
		break;
	}

	UWorld* World = Context->ExecutionSource.IsValid() ? Context->ExecutionSource->GetExecutionState().GetWorld() : nullptr;
	if (World && World->IsGameWorld())
	{
		if (const UP48PCGSeedWorldSubsystem* Coordinator = World->GetSubsystem<UP48PCGSeedWorldSubsystem>())
		{
			const FP48PCGGenerationContext WorldContext = Coordinator->GetGenerationContext();
			// Runtime generation is authoritative. Never allow stale editor ParamData
			// to replace the replicated seed on either the server or a client.
			OutContext = WorldContext;
			return WorldContext.IsValid();
		}
	}
	return Context->ExecutionSource.IsValid() || bFoundParamData;
}

int32 P48ReadNetworkSeed(FPCGContext* Context)
{
	FP48PCGGenerationContext GenerationContext;
	return P48ReadGenerationContext(Context, GenerationContext)
		? GenerationContext.Seed
		: (Context ? Context->GetSeed() : 0);
}
