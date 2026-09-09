#include "P48PCGPlayerSpawnSelectorSettings.h"
#include "../Common/P48PCGSeedHelpers.h"
#include "../Common/P48PCGSeedState.h"
#include "../../Utilities/P48MapPlayerCountResolver.h"

#include "../Common/P48PCGSpawnAttributeNames.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGContext.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "P48PCGPlayerSpawnSelectorSettings"

#if WITH_EDITOR
FText UP48PCGPlayerSpawnSelectorSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "P48 Player Spawn Selector");
}

FText UP48PCGPlayerSpawnSelectorSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Uses the current server player count and selects safe surface candidates, preferring unused islands and horizontal separation.");
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
	UWorld* World = Context->ExecutionSource.IsValid() ? Context->ExecutionSource->GetExecutionState().GetWorld() : nullptr;
	const bool bClient = World && World->GetNetMode() == NM_Client;
	AP48PCGSeedState* SeedState = nullptr;
	if (World && World->IsGameWorld())
	{
		for (TActorIterator<AP48PCGSeedState> It(World); It; ++It)
		{
			SeedState = *It;
			break;
		}
	}

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	const int32 CurrentPlayerCount = FP48MapPlayerCountResolver::Resolve(World);
	int32 RequestedCount = CurrentPlayerCount > 0 ? CurrentPlayerCount : FMath::Max(1, Settings->SelectionSettings.SpawnCount);
#if WITH_EDITORONLY_DATA
	if (Settings->bOverridePlayerCountForDebug)
	{
		RequestedCount = FMath::Max(1, Settings->DebugPlayerCount);
		UE_LOG(LogTemp, Display, TEXT("[P48PlayerSpawnDebug] ActualPlayerCount=%d OverridePlayerCount=%d"), CurrentPlayerCount, RequestedCount);
	}
#endif
	bool bLayoutReported = false;
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGBasePointData* InputPoints = Cast<UPCGBasePointData>(Input.Data);
		if (!InputPoints || !InputPoints->Metadata)
		{
			continue;
		}

		const FPCGMetadataAttribute<int32>* IslandAttribute = InputPoints->Metadata->GetConstTypedAttribute<int32>(P48PCGSpawnAttributeNames::IslandIndex);
		const FPCGMetadataAttribute<bool>* CanSpawnAttribute = InputPoints->Metadata->GetConstTypedAttribute<bool>(P48PCGSpawnAttributeNames::CanSpawnPlayer);
		if (!IslandAttribute)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingIslandAttribute", "Input does not contain IslandIndex from P48 Player Spawn Surface Points."));
			continue;
		}

		TArray<int32> Remaining;
		for (int32 Index = 0; Index < InputPoints->GetNumPoints(); ++Index)
		{
			if (!CanSpawnAttribute || CanSpawnAttribute->GetValueFromItemKey(InputPoints->GetMetadataEntry(Index)))
			{
				Remaining.Add(Index);
			}
		}

		TArray<int32> Selected;
		TSet<int32> SelectedIslands;
		const int32 TargetCount = FMath::Min(RequestedCount, Remaining.Num());
		if (!Remaining.IsEmpty())
		{
			FRandomStream Random(PCGHelpers::ComputeSeed(Settings->SelectionSettings.RandomSeed, P48ReadNetworkSeed(Context)));
			const int32 FirstRemainingIndex = Random.RandRange(0, Remaining.Num() - 1);
			const int32 SelectedPointIndex = Remaining[FirstRemainingIndex];
			Selected.Add(SelectedPointIndex);
			SelectedIslands.Add(IslandAttribute->GetValueFromItemKey(InputPoints->GetMetadataEntry(SelectedPointIndex)));
			Remaining.RemoveAtSwap(FirstRemainingIndex);
		}

		const float RequiredDistanceSquared = FMath::Square(FMath::Max(0.0f, Settings->SelectionSettings.MinSpawnDistance));
		while (Selected.Num() < TargetCount && !Remaining.IsEmpty())
		{
			int32 BestRemainingIndex = INDEX_NONE;
			float BestMinimumDistanceSquared = -1.0f;
			const bool bHasUnusedIsland = Remaining.ContainsByPredicate([&](const int32 PointIndex)
			{
				return !SelectedIslands.Contains(IslandAttribute->GetValueFromItemKey(InputPoints->GetMetadataEntry(PointIndex)));
			});
			for (int32 RemainingIndex = 0; RemainingIndex < Remaining.Num(); ++RemainingIndex)
			{
				const int32 CandidatePointIndex = Remaining[RemainingIndex];
				const int32 CandidateIsland = IslandAttribute->GetValueFromItemKey(InputPoints->GetMetadataEntry(CandidatePointIndex));
				if (bHasUnusedIsland && SelectedIslands.Contains(CandidateIsland))
				{
					continue;
				}
				const FVector CandidateLocation = InputPoints->GetTransform(CandidatePointIndex).GetLocation();
				float MinimumDistanceSquared = TNumericLimits<float>::Max();
				for (const int32 SelectedIndex : Selected)
				{
					const FVector SelectedLocation = InputPoints->GetTransform(SelectedIndex).GetLocation();
					MinimumDistanceSquared = FMath::Min(MinimumDistanceSquared, FVector2D::DistSquared(FVector2D(CandidateLocation), FVector2D(SelectedLocation)));
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
			const int32 SelectedPointIndex = Remaining[BestRemainingIndex];
			Selected.Add(SelectedPointIndex);
			SelectedIslands.Add(IslandAttribute->GetValueFromItemKey(InputPoints->GetMetadataEntry(SelectedPointIndex)));
			Remaining.RemoveAtSwap(BestRemainingIndex);
		}

		UPCGBasePointData* OutputPoints = FPCGContext::NewPointData_AnyThread(Context);
		FPCGInitializeFromDataParams InitializeParams(InputPoints);
		InitializeParams.bInheritSpatialData = false;
		OutputPoints->InitializeFromDataWithParams(InitializeParams);
		if (!Selected.IsEmpty() && !bClient)
		{
			UPCGBasePointData::SetPoints(InputPoints, OutputPoints, Selected, false);
			UPCGMetadata* OutputMetadata = OutputPoints->MutableMetadata();
			auto* SlotAttribute = OutputMetadata->CreateAttribute<int32>(P48PCGSpawnAttributeNames::SpawnSlotIndex, INDEX_NONE, false, false);
			auto* GenerationAttribute = OutputMetadata->CreateAttribute<int32>(P48PCGSpawnAttributeNames::GenerationId, 0, false, false);
			TPCGValueRange<FTransform> Transforms = OutputPoints->GetTransformValueRange();
			for (int32 Index = 0; Index < OutputPoints->GetNumPoints(); ++Index)
			{
				FTransform Transform = Transforms[Index];
				Transform.AddToTranslation(FVector(0.0, 0.0, Settings->SelectionSettings.SpawnHeight));
				Transforms[Index] = Transform;
				const PCGMetadataEntryKey Entry = OutputPoints->GetMetadataEntry(Index);
				SlotAttribute->SetValue(Entry, Index);
				GenerationAttribute->SetValue(Entry, SeedState ? SeedState->State.Revision : 0);
			}
		}

		if (!bClient && Selected.Num() < RequestedCount)
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("InsufficientSpawnPoints", "Selected {0} of {1} requested player spawn points. Reduce Min Spawn Distance or enable more islands for player spawning."), FText::AsNumber(Selected.Num()), FText::AsNumber(RequestedCount)));
		}
		if (SeedState && SeedState->HasAuthority())
		{
			SeedState->ReportPlayerStartLayout(RequestedCount, Selected.Num());
			bLayoutReported = true;
		}

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef(Input);
		Output.Data = OutputPoints;
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}

	if (SeedState && SeedState->HasAuthority() && !bLayoutReported)
	{
		SeedState->ReportPlayerStartLayout(RequestedCount, 0);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
