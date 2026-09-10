#include "P48PCGNetworkSeedSettings.h"

#include "P48PCGSeedHelpers.h"
#include "P48PCGSeedWorldSubsystem.h"
#include "PCGContext.h"
#include "PCGComponent.h"
#include "PCGParamData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Engine/World.h"
#include "Misc/Guid.h"

int32 UP48PCGNetworkSeedSettings::GenerateServerSeed()
{
	return FMath::Max(1, static_cast<int32>(GetTypeHash(FGuid::NewGuid()) & MAX_int32));
}

TArray<FPCGPinProperties> UP48PCGNetworkSeedSettings::OutputPinProperties() const
{
	return { FPCGPinProperties(P48PCGSeedNames::Seed, EPCGDataType::Param) };
}

FPCGElementPtr UP48PCGNetworkSeedSettings::CreateElement() const
{
	return MakeShared<FP48PCGNetworkSeedElement>();
}

bool FP48PCGNetworkSeedElement::ExecuteInternal(FPCGContext* Context) const
{
	const UP48PCGNetworkSeedSettings* Settings = Context->GetInputSettings<UP48PCGNetworkSeedSettings>();
	UWorld* World = Context->ExecutionSource.IsValid() ? Context->ExecutionSource->GetExecutionState().GetWorld() : nullptr;
	if (!World || !Settings)
	{
		return true;
	}

	FP48PCGGenerationContext GenerationContext;
	GenerationContext.Seed = Context->ExecutionSource->GetExecutionState().GetSeed();
	if (World->IsGameWorld())
	{
		UP48PCGSeedWorldSubsystem* Coordinator = World->GetSubsystem<UP48PCGSeedWorldSubsystem>();
		if (!Coordinator || !Coordinator->GetSnapshot().HasValidSeed())
		{
			return false;
		}
		Coordinator->RegisterConsumer(Cast<UPCGComponent>(Context->ExecutionSource.Get()));
		GenerationContext = Coordinator->GetGenerationContext();
		UE_LOG(LogTemp, Display, TEXT("[P48NetworkSeed] NetMode=%d Seed=%d Generation=%d Players=%d"), static_cast<int32>(World->GetNetMode()), GenerationContext.Seed, GenerationContext.GenerationId, GenerationContext.RequiredPlayerCount);
	}

	UPCGParamData* Data = NewObject<UPCGParamData>();
	const PCGMetadataEntryKey Entry = Data->Metadata->AddEntry();
	Data->Metadata->CreateAttribute<int32>(P48PCGSeedNames::Seed, GenerationContext.Seed, false, false)->SetValue(Entry, GenerationContext.Seed);
	Data->Metadata->CreateAttribute<int32>(P48PCGSeedNames::GenerationId, GenerationContext.GenerationId, false, false)->SetValue(Entry, GenerationContext.GenerationId);
	Data->Metadata->CreateAttribute<int32>(P48PCGSeedNames::RequiredPlayerCount, GenerationContext.RequiredPlayerCount, false, false)->SetValue(Entry, GenerationContext.RequiredPlayerCount);

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Pin = P48PCGSeedNames::Seed;
	Output.Data = Data;
	return true;
}
