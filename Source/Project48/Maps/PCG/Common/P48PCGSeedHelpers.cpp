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

	if (Context->ExecutionSource.IsValid())
	{
		// The component seed is the single editable/runtime-overridable map seed.
		OutContext.Seed = Context->ExecutionSource->GetExecutionState().GetSeed();
	}

	UWorld* World = Context->ExecutionSource.IsValid() ? Context->ExecutionSource->GetExecutionState().GetWorld() : nullptr;
	if (World && World->IsGameWorld())
	{
		if (const UP48PCGSeedWorldSubsystem* Coordinator = World->GetSubsystem<UP48PCGSeedWorldSubsystem>())
		{
			const FP48PCGGenerationContext WorldContext = Coordinator->GetGenerationContext();
			// Runtime generation is authoritative. Never allow stale editor ParamData
			// to replace the replicated seed on either the server or a client.
			OutContext.Seed = WorldContext.Seed;
			if (OutContext.GenerationId <= 0)
			{
				OutContext.GenerationId = WorldContext.GenerationId;
			}
			if (OutContext.RequiredPlayerCount <= 0)
			{
				OutContext.RequiredPlayerCount = WorldContext.RequiredPlayerCount;
			}
			return WorldContext.IsValid() || bFoundParamData;
		}
	}
	return Context->ExecutionSource.IsValid() || bFoundParamData;
}

int32 P48ReadNetworkSeed(FPCGContext* Context)
{
	if (Context && Context->ExecutionSource.IsValid())
	{
		return Context->ExecutionSource->GetExecutionState().GetSeed();
	}
	return Context ? Context->GetSeed() : 0;
}
