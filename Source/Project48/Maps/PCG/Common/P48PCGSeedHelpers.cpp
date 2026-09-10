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

	bool bFoundSeed = false;
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
			bFoundSeed = true;
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
			if (!bFoundSeed)
			{
				OutContext.Seed = WorldContext.Seed;
			}
			if (OutContext.GenerationId <= 0)
			{
				OutContext.GenerationId = WorldContext.GenerationId;
			}
			if (OutContext.RequiredPlayerCount <= 0)
			{
				OutContext.RequiredPlayerCount = WorldContext.RequiredPlayerCount;
			}
			return WorldContext.IsValid() || bFoundSeed;
		}
	}
	return bFoundSeed;
}

int32 P48ReadNetworkSeed(FPCGContext* Context)
{
	FP48PCGGenerationContext GenerationContext;
	return P48ReadGenerationContext(Context, GenerationContext) ? GenerationContext.Seed : Context->GetSeed();
}
