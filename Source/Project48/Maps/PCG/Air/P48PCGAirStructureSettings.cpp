#include "P48PCGAirStructureSettings.h"

#include "PCGComponent.h"
#include "../Common/P48PCGSeedHelpers.h"

#include "../Common/P48PCGSpawnAttributeNames.h"
#include "Data/PCGPointData.h"
#include "Engine/StaticMesh.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGContext.h"

#define LOCTEXT_NAMESPACE "P48PCGAirStructureSettings"

const FName UP48PCGAirStructureSettings::StaticMeshOutputLabel(TEXT("StaticMeshPoints"));
const FName UP48PCGAirStructureSettings::ActorOutputLabel(TEXT("ActorPoints"));
const FName UP48PCGAirStructureSettings::AnchorOutputLabel(TEXT("ConnectionAnchors"));

namespace P48AirStructure
{
	enum class ESpawnType : uint8
	{
		StaticMesh,
		Actor
	};

	struct FPreparedEntry
	{
		ESpawnType Type = ESpawnType::StaticMesh;
		TObjectPtr<UStaticMesh> Mesh = nullptr;
		TSubclassOf<AActor> ActorClass;
		FVector Scale = FVector::OneVector;
		FVector BoundsOrigin = FVector::ZeroVector;
		FVector BoundsExtent = FVector::ZeroVector;
		float Weight = 0.0f;
		float Radius = 0.0f;
		float MinGap = 0.0f;
		float AnchorHeight = 0.0f;
		bool bCanConnectBridge = false;
		bool bCanSpawnPlayer = false;
	};

	struct FPlacedEntry
	{
		const FPreparedEntry* Entry = nullptr;
		FVector LocalLocation = FVector::ZeroVector;
		float Yaw = 0.0f;
		int32 Seed = 0;
		int32 IslandIndex = INDEX_NONE;
	};

	void PrepareEntries(const UP48PCGAirStructureSettings& Settings, TArray<FPreparedEntry>& OutEntries, float& OutTotalWeight)
	{
		OutEntries.Reset();
		OutTotalWeight = 0.0f;

		for (const FP48WeightedStaticMeshSpawn& Source : Settings.StaticMeshes)
		{
			if (!IsValid(Source.Mesh) || Source.Weight <= 0.0f)
			{
				continue;
			}

			const FBoxSphereBounds LocalBounds = Source.Mesh->GetBounds();
			const FVector AbsoluteScale = Source.Scale.GetAbs();
			FPreparedEntry& Entry = OutEntries.Emplace_GetRef();
			Entry.Type = ESpawnType::StaticMesh;
			Entry.Mesh = Source.Mesh;
			Entry.Scale = Source.Scale;
			Entry.BoundsOrigin = LocalBounds.Origin;
			Entry.BoundsExtent = LocalBounds.BoxExtent;
			Entry.Weight = Source.Weight;
			Entry.Radius = Source.PlacementRadiusOverride > 0.0f ? Source.PlacementRadiusOverride : FVector2D(LocalBounds.BoxExtent.X * AbsoluteScale.X, LocalBounds.BoxExtent.Y * AbsoluteScale.Y).Length();
			Entry.MinGap = FMath::Max(0.0f, Source.MinGap);
			Entry.AnchorHeight = Source.bOverrideBridgeAnchorHeight ? Source.BridgeAnchorHeight : LocalBounds.Origin.Z * Source.Scale.Z + LocalBounds.BoxExtent.Z * FMath::Abs(Source.Scale.Z);
			Entry.bCanConnectBridge = Source.bCanConnectBridge;
			Entry.bCanSpawnPlayer = Source.bCanSpawnPlayer;
			OutTotalWeight += Entry.Weight;
		}

		for (const FP48WeightedActorSpawn& Source : Settings.ActorClasses)
		{
			if (!Source.ActorClass || Source.Weight <= 0.0f)
			{
				continue;
			}

			FPreparedEntry& Entry = OutEntries.Emplace_GetRef();
			Entry.Type = ESpawnType::Actor;
			Entry.ActorClass = Source.ActorClass;
			Entry.Scale = Source.Scale;
			Entry.BoundsExtent = FVector(Source.PlacementRadius, Source.PlacementRadius, FMath::Max(Source.BridgeAnchorHeight, 100.0f));
			Entry.Weight = Source.Weight;
			Entry.Radius = FMath::Max(1.0f, Source.PlacementRadius);
			Entry.MinGap = FMath::Max(0.0f, Source.MinGap);
			Entry.AnchorHeight = Source.BridgeAnchorHeight;
			Entry.bCanConnectBridge = Source.bCanConnectBridge;
			Entry.bCanSpawnPlayer = Source.bCanSpawnPlayer;
			OutTotalWeight += Entry.Weight;
		}
	}

	const FPreparedEntry* SelectWeighted(const TArray<FPreparedEntry>& Entries, const float TotalWeight, FRandomStream& Random)
	{
		if (Entries.IsEmpty() || TotalWeight <= 0.0f)
		{
			return nullptr;
		}

		const float Selection = Random.FRandRange(0.0f, TotalWeight);
		float Accumulated = 0.0f;
		for (const FPreparedEntry& Entry : Entries)
		{
			Accumulated += Entry.Weight;
			if (Selection <= Accumulated)
			{
				return &Entry;
			}
		}

		return &Entries.Last();
	}

	bool Overlaps(const FPreparedEntry& Candidate, const FVector& CandidateLocation, const TArray<FPlacedEntry>& Placed, const float GlobalMinGap)
	{
		for (const FPlacedEntry& Existing : Placed)
		{
			const float RequiredDistance = Candidate.Radius + Existing.Entry->Radius + FMath::Max(Candidate.MinGap, Existing.Entry->MinGap) + GlobalMinGap;
			if (FVector2D::DistSquared(FVector2D(CandidateLocation), FVector2D(Existing.LocalLocation)) < FMath::Square(RequiredDistance))
			{
				return true;
			}
		}

		return false;
	}

	bool IsBridgeValid(const FVector& Start, const FVector& End, const FP48BridgeConnectionRules& Rules)
	{
		const FVector Difference = End - Start;
		const float HorizontalDistance = FVector2D(Difference.X, Difference.Y).Length();
		const float HeightDifference = FMath::Abs(Difference.Z);
		if (HorizontalDistance < Rules.MinHorizontalDistance || HeightDifference > Rules.MaxHeightDifference || Difference.Length() > Rules.MaxBridgeLength)
		{
			return false;
		}

		const float SlopeAngle = FMath::RadiansToDegrees(FMath::Atan2(HeightDifference, HorizontalDistance));
		return SlopeAngle <= Rules.MaxSlopeAngle;
	}

	bool ConnectsToLayout(const FPreparedEntry& Candidate, const FVector& CandidateLocation, const TArray<FPlacedEntry>& Placed, const FP48AirStructureGenerationSettings& Settings)
	{
		if (Placed.IsEmpty())
		{
			return true;
		}

		if (!Candidate.bCanConnectBridge)
		{
			return false;
		}

		const FVector CandidateAnchor = CandidateLocation + FVector(0.0, 0.0, Candidate.AnchorHeight);
		for (const FPlacedEntry& Existing : Placed)
		{
			if (Existing.Entry->bCanConnectBridge)
			{
				const FVector ExistingAnchor = Existing.LocalLocation + FVector(0.0, 0.0, Existing.Entry->AnchorHeight);
				if (IsBridgeValid(CandidateAnchor, ExistingAnchor, Settings.ConnectionRules))
				{
					return true;
				}
			}
		}

		return false;
	}

	float QuantizeHeight(const float Height, const float MinHeight, const float MaxHeight, const float HeightStep)
	{
		if (HeightStep <= UE_KINDA_SMALL_NUMBER)
		{
			return FMath::Clamp(Height, MinHeight, MaxHeight);
		}
		const float Level = FMath::RoundToFloat((Height - MinHeight) / HeightStep);
		return FMath::Clamp(MinHeight + Level * HeightStep, MinHeight, MaxHeight);
	}

	const FPlacedEntry* SelectConnectionParent(const TArray<FPlacedEntry>& Placed, FRandomStream& Random)
	{
		TArray<const FPlacedEntry*, TInlineAllocator<32>> Candidates;
		for (const FPlacedEntry& Item : Placed)
		{
			if (Item.Entry && Item.Entry->bCanConnectBridge)
			{
				Candidates.Add(&Item);
			}
		}
		return Candidates.IsEmpty() ? nullptr : Candidates[Random.RandRange(0, Candidates.Num() - 1)];
	}

	bool GenerateSkyIslandLocation(const FPreparedEntry& Candidate, const TArray<FPlacedEntry>& Placed, const FP48AirStructureGenerationSettings& Settings, const FVector2D& HalfMapSize, const float MinHeight, const float MaxHeight, FRandomStream& Random, FVector& OutLocation)
	{
		const float AvailableX = HalfMapSize.X - Candidate.Radius;
		const float AvailableY = HalfMapSize.Y - Candidate.Radius;
		if (AvailableX < 0.0f || AvailableY < 0.0f)
		{
			return false;
		}
		if (Placed.IsEmpty())
		{
			OutLocation = FVector(0.0f, 0.0f, QuantizeHeight(Random.FRandRange(MinHeight, MaxHeight), MinHeight, MaxHeight, Settings.HeightStep));
			return true;
		}
		if (!Candidate.bCanConnectBridge)
		{
			return false;
		}
		const FPlacedEntry* Parent = SelectConnectionParent(Placed, Random);
		if (!Parent)
		{
			return false;
		}
		const float RequiredDistance = Candidate.Radius + Parent->Entry->Radius + FMath::Max(Candidate.MinGap, Parent->Entry->MinGap) + FMath::Max(0.0f, Settings.GlobalMinGap);
		const float MaximumDistance = FMath::Min(FMath::Max(1.0f, Settings.MaxIslandCenterDistance), Settings.ConnectionRules.MaxBridgeLength);
		if (RequiredDistance > MaximumDistance)
		{
			return false;
		}
		const float Distance = Random.FRandRange(RequiredDistance, MaximumDistance);
		const float Angle = Random.FRandRange(0.0f, UE_TWO_PI);
		const FVector2D HorizontalOffset(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance);
		const FVector2D HorizontalLocation = FVector2D(Parent->LocalLocation) + HorizontalOffset;
		if (FMath::Abs(HorizontalLocation.X) > AvailableX || FMath::Abs(HorizontalLocation.Y) > AvailableY)
		{
			return false;
		}
		float Height = Parent->LocalLocation.Z;
		if (Settings.HeightStep > UE_KINDA_SMALL_NUMBER)
		{
			const float Direction = Random.RandRange(0, 1) == 0 ? -1.0f : 1.0f;
			const float ParentAnchorHeight = Parent->LocalLocation.Z + Parent->Entry->AnchorHeight;
			Height = ParentAnchorHeight + Direction * Settings.HeightStep - Candidate.AnchorHeight;
			if (Height < MinHeight || Height > MaxHeight)
			{
				Height = ParentAnchorHeight - Direction * Settings.HeightStep - Candidate.AnchorHeight;
			}
		}
		OutLocation = FVector(HorizontalLocation.X, HorizontalLocation.Y, FMath::Clamp(Height, MinHeight, MaxHeight));
		return true;
	}

	UPCGBasePointData* CreatePointData(FPCGContext* Context, const TArray<FPlacedEntry>& Placed, const ESpawnType Type, const FTransform& SourceTransform)
	{
		TArray<const FPlacedEntry*> Matching;
		for (const FPlacedEntry& Item : Placed)
		{
			if (Item.Entry->Type == Type)
			{
				Matching.Add(&Item);
			}
		}

		UPCGBasePointData* PointData = FPCGContext::NewPointData_AnyThread(Context);
		PointData->SetNumPoints(Matching.Num(), false);
		PointData->AllocateProperties(EPCGPointNativeProperties::All);
		UPCGMetadata* Metadata = PointData->MutableMetadata();
		FPCGMetadataAttribute<FSoftObjectPath>* MeshAttribute = Type == ESpawnType::StaticMesh ? Metadata->CreateAttribute<FSoftObjectPath>(P48PCGSpawnAttributeNames::Mesh, FSoftObjectPath(), false, false) : nullptr;
		FPCGMetadataAttribute<FSoftClassPath>* ActorClassAttribute = Type == ESpawnType::Actor ? Metadata->CreateAttribute<FSoftClassPath>(P48PCGSpawnAttributeNames::ActorClass, FSoftClassPath(), false, false) : nullptr;
		FPCGMetadataAttribute<int32>* IslandIndexAttribute = Metadata->CreateAttribute<int32>(P48PCGSpawnAttributeNames::IslandIndex, INDEX_NONE, false, false);
		FPCGMetadataAttribute<bool>* PlayerAttribute = Type == ESpawnType::StaticMesh ? Metadata->CreateAttribute<bool>(P48PCGSpawnAttributeNames::CanSpawnPlayer, false, false, false) : nullptr;

		FPCGPointValueRanges Ranges(PointData, false);
		for (int32 PointIndex = 0; PointIndex < Matching.Num(); ++PointIndex)
		{
			const FPlacedEntry& Item = *Matching[PointIndex];
			FPCGPoint Point;
			Point.Transform = FTransform(FRotator(0.0f, Item.Yaw, 0.0f), Item.LocalLocation, Item.Entry->Scale) * SourceTransform;
			Point.Density = 1.0f;
			Point.BoundsMin = Item.Entry->BoundsOrigin - Item.Entry->BoundsExtent;
			Point.BoundsMax = Item.Entry->BoundsOrigin + Item.Entry->BoundsExtent;
			Point.Steepness = 1.0f;
			Point.Seed = Item.Seed;
			Point.MetadataEntry = Metadata->AddEntry();
			if (MeshAttribute)
			{
				MeshAttribute->SetValue(Point.MetadataEntry, FSoftObjectPath(Item.Entry->Mesh));
			}
			if (ActorClassAttribute)
			{
				ActorClassAttribute->SetValue(Point.MetadataEntry, FSoftClassPath(Item.Entry->ActorClass.Get()));
			}
			IslandIndexAttribute->SetValue(Point.MetadataEntry, Item.IslandIndex);
			if (PlayerAttribute)
			{
				PlayerAttribute->SetValue(Point.MetadataEntry, Item.Entry->bCanSpawnPlayer);
			}
			Ranges.SetFromPoint(PointIndex, Point);
		}

		return PointData;
	}

	UPCGBasePointData* CreateAnchorData(FPCGContext* Context, const TArray<FPlacedEntry>& Placed, const FTransform& SourceTransform)
	{
		TArray<const FPlacedEntry*> Anchors;
		for (const FPlacedEntry& Item : Placed)
		{
			if (Item.Entry->bCanConnectBridge)
			{
				Anchors.Add(&Item);
			}
		}

		UPCGBasePointData* PointData = FPCGContext::NewPointData_AnyThread(Context);
		PointData->SetNumPoints(Anchors.Num(), false);
		PointData->AllocateProperties(EPCGPointNativeProperties::All);
		UPCGMetadata* Metadata = PointData->MutableMetadata();
		FPCGMetadataAttribute<int32>* IslandIndexAttribute = Metadata->CreateAttribute<int32>(P48PCGSpawnAttributeNames::IslandIndex, INDEX_NONE, false, false);
		FPCGMetadataAttribute<float>* RadiusAttribute = Metadata->CreateAttribute<float>(P48PCGSpawnAttributeNames::PlacementRadius, 0.0f, true, false);
		FPCGMetadataAttribute<bool>* PlayerAttribute = Metadata->CreateAttribute<bool>(P48PCGSpawnAttributeNames::CanSpawnPlayer, false, false, false);

		FPCGPointValueRanges Ranges(PointData, false);
		for (int32 PointIndex = 0; PointIndex < Anchors.Num(); ++PointIndex)
		{
			const FPlacedEntry& Item = *Anchors[PointIndex];
			FPCGPoint Point;
			Point.Transform = FTransform(FRotator(0.0f, Item.Yaw, 0.0f), Item.LocalLocation + FVector(0.0, 0.0, Item.Entry->AnchorHeight)) * SourceTransform;
			Point.Density = 1.0f;
			Point.Seed = Item.Seed;
			Point.MetadataEntry = Metadata->AddEntry();
			IslandIndexAttribute->SetValue(Point.MetadataEntry, Item.IslandIndex);
			RadiusAttribute->SetValue(Point.MetadataEntry, Item.Entry->Radius);
			PlayerAttribute->SetValue(Point.MetadataEntry, Item.Entry->bCanSpawnPlayer);
			Ranges.SetFromPoint(PointIndex, Point);
		}

		return PointData;
	}
}

#if WITH_EDITOR
FText UP48PCGAirStructureSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "P48 Air Structure Generator");
}

FText UP48PCGAirStructureSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Places weighted Static Meshes and Actor Blueprints as a bridge-connectable sky-island layout.");
}
#endif

TArray<FPCGPinProperties> UP48PCGAirStructureSettings::InputPinProperties() const
{
	return { FPCGPinProperties(P48PCGSeedNames::InputPin, EPCGDataType::Param) };
}

TArray<FPCGPinProperties> UP48PCGAirStructureSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(StaticMeshOutputLabel, EPCGDataType::Point);
	Pins.Emplace(ActorOutputLabel, EPCGDataType::Point);
	Pins.Emplace(AnchorOutputLabel, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UP48PCGAirStructureSettings::CreateElement() const
{
	return MakeShared<FP48PCGAirStructureElement>();
}

bool FP48PCGAirStructureElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	const UP48PCGAirStructureSettings* Settings = Context->GetInputSettings<UP48PCGAirStructureSettings>();
	if (!Settings)
	{
		return true;
	}

	TArray<P48AirStructure::FPreparedEntry> Entries;
	float TotalWeight = 0.0f;
	P48AirStructure::PrepareEntries(*Settings, Entries, TotalWeight);
	if (Entries.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoSpawnEntries", "No valid Static Mesh or Actor Blueprint entries were provided."));
		return true;
	}

	const FP48AirStructureGenerationSettings& Rules = Settings->GenerationSettings;
	const int32 TargetCount = FMath::Max(1, Rules.TargetCount);
	const FVector2D HalfMapSize = Rules.MapSize.GetAbs() * 0.5f;
	const float MinHeight = FMath::Min(Rules.HeightRange.X, Rules.HeightRange.Y);
	const float MaxHeight = FMath::Max(Rules.HeightRange.X, Rules.HeightRange.Y);
	const int32 BaseSeed = PCGHelpers::ComputeSeed(Rules.RandomSeed, P48ReadNetworkSeed(Context));
	TArray<P48AirStructure::FPlacedEntry> BestLayout;

	for (int32 MapAttempt = 0; MapAttempt < FMath::Max(1, Rules.MaxMapGenerationAttempts); ++MapAttempt)
	{
		FRandomStream Random(PCGHelpers::ComputeSeed(BaseSeed, MapAttempt));
		TArray<P48AirStructure::FPlacedEntry> Layout;
		Layout.Reserve(TargetCount);

		for (int32 IslandIndex = 0; IslandIndex < TargetCount; ++IslandIndex)
		{
			bool bPlaced = false;
			for (int32 PlacementAttempt = 0; PlacementAttempt < FMath::Max(1, Rules.MaxPlacementAttemptsPerStructure); ++PlacementAttempt)
			{
				const P48AirStructure::FPreparedEntry* Entry = P48AirStructure::SelectWeighted(Entries, TotalWeight, Random);
				if (!Entry)
				{
					continue;
				}

				FVector Location = FVector::ZeroVector;
				if (Rules.PlacementMode == EP48AirPlacementMode::SkyIsland)
				{
					if (!P48AirStructure::GenerateSkyIslandLocation(*Entry, Layout, Rules, HalfMapSize, MinHeight, MaxHeight, Random, Location))
					{
						continue;
					}
				}
				else
				{
					const float AvailableX = HalfMapSize.X - Entry->Radius;
					const float AvailableY = HalfMapSize.Y - Entry->Radius;
					if (AvailableX < 0.0f || AvailableY < 0.0f)
					{
						continue;
					}
					Location = FVector(Random.FRandRange(-AvailableX, AvailableX), Random.FRandRange(-AvailableY, AvailableY), Random.FRandRange(MinHeight, MaxHeight));
				}
				if (P48AirStructure::Overlaps(*Entry, Location, Layout, FMath::Max(0.0f, Rules.GlobalMinGap)))
				{
					continue;
				}

				if (Rules.PlacementMode == EP48AirPlacementMode::SkyIsland && Rules.bRequireConnectableLayout && !P48AirStructure::ConnectsToLayout(*Entry, Location, Layout, Rules))
				{
					continue;
				}

				P48AirStructure::FPlacedEntry& Placed = Layout.Emplace_GetRef();
				Placed.Entry = Entry;
				Placed.LocalLocation = Location;
				Placed.Yaw = Random.FRandRange(0.0f, 360.0f);
				Placed.Seed = static_cast<int32>(Random.GetUnsignedInt());
				Placed.IslandIndex = IslandIndex;
				bPlaced = true;
				break;
			}

			if (!bPlaced)
			{
				break;
			}
		}

		if (Layout.Num() > BestLayout.Num())
		{
			BestLayout = MoveTemp(Layout);
		}

		if (BestLayout.Num() == TargetCount)
		{
			break;
		}
	}

	if (BestLayout.Num() < TargetCount)
	{
		PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("IncompleteLayout", "Placed {0} of {1} requested air structures. Increase map size or placement attempts, or relax spacing and bridge limits."), FText::AsNumber(BestLayout.Num()), FText::AsNumber(TargetCount)));
	}

	FTransform SourceTransform = FTransform::Identity;
	if (Context->ExecutionSource.IsValid())
	{
		SourceTransform = Context->ExecutionSource->GetExecutionState().GetTransform();
		SourceTransform.SetScale3D(FVector::OneVector);
	}

	auto AddOutput = [Context](UPCGData* Data, const FName Pin)
	{
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = Data;
		Output.Pin = Pin;
	};

	UWorld* World = Context->ExecutionSource.IsValid() ? Context->ExecutionSource->GetExecutionState().GetWorld() : nullptr;
	const bool bClient = World && World->GetNetMode() == NM_Client;

	AddOutput(P48AirStructure::CreatePointData(Context, BestLayout, P48AirStructure::ESpawnType::StaticMesh, SourceTransform), UP48PCGAirStructureSettings::StaticMeshOutputLabel);

	if (!bClient)
	{
		AddOutput(P48AirStructure::CreatePointData(Context, BestLayout, P48AirStructure::ESpawnType::Actor, SourceTransform), UP48PCGAirStructureSettings::ActorOutputLabel);
	}

	AddOutput(P48AirStructure::CreateAnchorData(Context, BestLayout, SourceTransform), UP48PCGAirStructureSettings::AnchorOutputLabel);
	return true;
}

#undef LOCTEXT_NAMESPACE
