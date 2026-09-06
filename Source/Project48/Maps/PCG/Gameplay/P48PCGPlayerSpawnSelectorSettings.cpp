#include "P48PCGPlayerSpawnSelectorSettings.h"
#include "../Common/P48PCGSeedHelpers.h"

#include "../Common/P48PCGSpawnAttributeNames.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGContext.h"

#define LOCTEXT_NAMESPACE "P48PCGPlayerSpawnSelectorSettings"

#if WITH_EDITOR
FText UP48PCGPlayerSpawnSelectorSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "P48 Player Spawn Selector");
}

FText UP48PCGPlayerSpawnSelectorSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Selects player-safe sky-island anchors with a minimum distance between spawn points.");
}
#endif

TArray<FPCGPinProperties> UP48PCGPlayerSpawnSelectorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	FPCGPinProperties& Input = Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
	Input.SetRequiredPin();
	Pins.Emplace(P48PCGSeedNames::InputPin, EPCGDataType::Param);
	return Pins;
}

TArray<FPCGPinProperties> UP48PCGPlayerSpawnSelectorSettings::OutputPinProperties() const
{
	return Super::DefaultPointOutputPinProperties();
}

FPCGElementPtr UP48PCGPlayerSpawnSelectorSettings::CreateElement() const
{
	return MakeShared<FP48PCGPlayerSpawnSelectorElement>();
}

bool FP48PCGPlayerSpawnSelectorElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	const UP48PCGPlayerSpawnSelectorSettings* Settings = Context->GetInputSettings<UP48PCGPlayerSpawnSelectorSettings>();
	if (!Settings)
	{
		return true;
	}

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGBasePointData* InputPoints = Cast<UPCGBasePointData>(Input.Data);
		if (!InputPoints || !InputPoints->Metadata)
		{
			continue;
		}

		const FPCGMetadataAttribute<bool>* CanSpawnAttribute = InputPoints->Metadata->GetConstTypedAttribute<bool>(P48PCGSpawnAttributeNames::CanSpawnPlayer);
		if (!CanSpawnAttribute)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingSpawnAttribute", "Input does not contain the CanSpawnPlayer attribute from P48 Air Structure Generator."));
			continue;
		}

		TArray<int32> Remaining;
		for (int32 Index = 0; Index < InputPoints->GetNumPoints(); ++Index)
		{
			if (CanSpawnAttribute->GetValueFromItemKey(InputPoints->GetMetadataEntry(Index)))
			{
				Remaining.Add(Index);
			}
		}

		TArray<int32> Selected;
		const int32 TargetCount = FMath::Min(FMath::Max(1, Settings->SelectionSettings.SpawnCount), Remaining.Num());
		if (!Remaining.IsEmpty())
		{
			FRandomStream Random(PCGHelpers::ComputeSeed(Settings->SelectionSettings.RandomSeed, P48ReadNetworkSeed(Context)));
			const int32 FirstRemainingIndex = Random.RandRange(0, Remaining.Num() - 1);
			Selected.Add(Remaining[FirstRemainingIndex]);
			Remaining.RemoveAtSwap(FirstRemainingIndex);
		}

		const float RequiredDistanceSquared = FMath::Square(FMath::Max(0.0f, Settings->SelectionSettings.MinSpawnDistance));
		while (Selected.Num() < TargetCount && !Remaining.IsEmpty())
		{
			int32 BestRemainingIndex = INDEX_NONE;
			float BestMinimumDistanceSquared = -1.0f;
			for (int32 RemainingIndex = 0; RemainingIndex < Remaining.Num(); ++RemainingIndex)
			{
				const FVector CandidateLocation = InputPoints->GetTransform(Remaining[RemainingIndex]).GetLocation();
				float MinimumDistanceSquared = TNumericLimits<float>::Max();
				for (const int32 SelectedIndex : Selected)
				{
					MinimumDistanceSquared = FMath::Min(MinimumDistanceSquared, FVector::DistSquared(CandidateLocation, InputPoints->GetTransform(SelectedIndex).GetLocation()));
				}
				if (MinimumDistanceSquared >= RequiredDistanceSquared && MinimumDistanceSquared > BestMinimumDistanceSquared)
				{
					BestMinimumDistanceSquared = MinimumDistanceSquared;
					BestRemainingIndex = RemainingIndex;
				}
			}

			if (BestRemainingIndex == INDEX_NONE)
			{
				break;
			}
			Selected.Add(Remaining[BestRemainingIndex]);
			Remaining.RemoveAtSwap(BestRemainingIndex);
		}

		UPCGBasePointData* OutputPoints = FPCGContext::NewPointData_AnyThread(Context);
		FPCGInitializeFromDataParams InitializeParams(InputPoints);
		InitializeParams.bInheritSpatialData = false;
		OutputPoints->InitializeFromDataWithParams(InitializeParams);
		if (!Selected.IsEmpty())
		{
			UPCGBasePointData::SetPoints(InputPoints, OutputPoints, Selected, false);
			TPCGValueRange<FTransform> Transforms = OutputPoints->GetTransformValueRange();
			for (int32 Index = 0; Index < OutputPoints->GetNumPoints(); ++Index)
			{
				FTransform Transform = Transforms[Index];
				Transform.AddToTranslation(FVector(0.0, 0.0, Settings->SelectionSettings.SpawnHeight));
				Transforms[Index] = Transform;
			}
		}

		if (Selected.Num() < TargetCount)
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("InsufficientSpawnPoints", "Selected {0} of {1} player spawn points. Reduce Min Spawn Distance or enable more islands for player spawning."), FText::AsNumber(Selected.Num()), FText::AsNumber(TargetCount)));
		}

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef(Input);
		Output.Data = OutputPoints;
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
