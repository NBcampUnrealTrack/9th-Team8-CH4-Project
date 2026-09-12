#include "P48PCGItemSpawnPointSettings.h"

#include "Project48/Maps/PCG/Common/P48PCGSeedHelpers.h"
#include "Project48/Maps/PCG/Common/P48PCGSpawnAttributeNames.h"
#include "Project48/Maps/PCG/Surface/P48IslandSurfaceSampler.h"
#include "Data/PCGPointData.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGContext.h"
#include "PCGModule.h"

#define LOCTEXT_NAMESPACE "P48ItemSpawnPoint"

namespace P48ItemSpawnPoint
{
	constexpr int32 SupportDirectionCount = 8;

	bool IsSafeSurface(
		UWorld* World,
		UStaticMesh* Mesh,
		const FTransform& IslandTransform,
		const FBox& Bounds,
		const FVector& Candidate,
		const float SafeRadius,
		const double MinNormalZ,
		FHitResult& OutHit)
	{
		if (!FP48IslandSurfaceSampler::TraceTop(
			World,
			Mesh,
			IslandTransform,
			Bounds,
			Candidate,
			OutHit,
			nullptr,
			true) || OutHit.ImpactNormal.Z < MinNormalZ)
		{
			return false;
		}

		if (SafeRadius <= 0.0f)
		{
			return true;
		}

		for (int32 DirectionIndex = 0; DirectionIndex < SupportDirectionCount; ++DirectionIndex)
		{
			const double Angle = 2.0 * PI * DirectionIndex / SupportDirectionCount;
			const FVector Offset(
				FMath::Cos(Angle) * SafeRadius,
				FMath::Sin(Angle) * SafeRadius,
				0.0f);
			FHitResult SupportHit;
			if (!FP48IslandSurfaceSampler::TraceTop(
				World,
				Mesh,
				IslandTransform,
				Bounds,
				OutHit.ImpactPoint + Offset,
				SupportHit,
				nullptr,
				true) ||
				SupportHit.ImpactNormal.Z < MinNormalZ ||
				FMath::Abs(SupportHit.ImpactPoint.Z - OutHit.ImpactPoint.Z) > SafeRadius)
			{
				return false;
			}
		}

		return true;
	}
}

#if WITH_EDITOR
FText UP48PCGItemSpawnPointSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("Title", "P48 Item Spawn Point");
}

FText UP48PCGItemSpawnPointSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Creates one safe item spawn point per island, preferring the top surface nearest the island center.");
}
#endif

TArray<FPCGPinProperties> UP48PCGItemSpawnPointSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	Pins.Emplace(P48PCGSeedNames::InputPin, EPCGDataType::Param);
	return Pins;
}

TArray<FPCGPinProperties> UP48PCGItemSpawnPointSettings::OutputPinProperties() const
{
	return Super::DefaultPointOutputPinProperties();
}

FPCGElementPtr UP48PCGItemSpawnPointSettings::CreateElement() const
{
	return MakeShared<FP48PCGItemSpawnPointElement>();
}

bool FP48PCGItemSpawnPointElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	const auto* Settings = Context->GetInputSettings<UP48PCGItemSpawnPointSettings>();
	UWorld* World = Context->ExecutionSource.IsValid()
		? Context->ExecutionSource->GetExecutionState().GetWorld()
		: nullptr;
	if (!Settings || !World)
	{
		return true;
	}

	auto* ItemContext = static_cast<FP48PCGItemSpawnPointContext*>(Context);
	if (World->IsGameWorld())
	{
		if (!ItemContext->bCollisionWaitStarted)
		{
			ItemContext->bCollisionWaitStarted = true;
			ItemContext->CollisionWaitStartFrame = GFrameCounter;
		}
		if (GFrameCounter <= ItemContext->CollisionWaitStartFrame)
		{
			ItemContext->bIsPaused = true;
			FPCGModule::GetPCGModuleChecked().ExecuteNextTick(
				[ContextHandle = ItemContext->GetOrCreateHandle()]()
				{
					FPCGContext::FSharedContext<FP48PCGItemSpawnPointContext> SharedContext(ContextHandle);
					if (FP48PCGItemSpawnPointContext* PendingContext = SharedContext.Get())
					{
						PendingContext->bIsPaused = false;
					}
				});
			return false;
		}
	}

	FP48PCGGenerationContext GenerationContext;
	P48ReadGenerationContext(Context, GenerationContext);
	const int32 NetworkSeed = GenerationContext.Seed != 0 ? GenerationContext.Seed : Context->GetSeed();
	const float Step = FMath::Max(50.0f, Settings->SearchStep);
	const float SearchRadius = FMath::Max(0.0f, Settings->MaxSearchRadius);
	const int32 RingCount = FMath::CeilToInt(SearchRadius / Step);
	const int32 SamplesPerRing = FMath::Clamp(Settings->SamplesPerRing, 4, 32);
	const float SafeRadius = FMath::Max(0.0f, Settings->SafeRadius);
	const double MinNormalZ = FMath::Cos(
		FMath::DegreesToRadians(FMath::Clamp(Settings->MaxSurfaceSlope, 0.0f, 80.0f)));

	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
	{
		const UPCGBasePointData* Islands = Cast<UPCGBasePointData>(Input.Data);
		if (!Islands || !Islands->Metadata)
		{
			continue;
		}

		const auto* MeshAttribute =
			Islands->Metadata->GetConstTypedAttribute<FSoftObjectPath>(P48PCGSpawnAttributeNames::Mesh);
		const auto* IslandAttribute =
			Islands->Metadata->GetConstTypedAttribute<int32>(P48PCGSpawnAttributeNames::IslandIndex);
		if (!MeshAttribute || !IslandAttribute)
		{
			PCGE_LOG(
				Error,
				GraphAndLog,
				LOCTEXT("MissingAttributes", "Item Spawn Point requires Mesh and IslandIndex attributes from the island Static Mesh Spawner output."));
			continue;
		}

		UPCGBasePointData* Output = FPCGContext::NewPointData_AnyThread(Context);
		UPCGMetadata* Metadata = Output->MutableMetadata();
		auto* OutputIslandIds = Metadata->CreateAttribute<int32>(
			P48PCGSpawnAttributeNames::IslandIndex,
			INDEX_NONE,
			false,
			false);
		auto* OutputNormals = Metadata->CreateAttribute<FVector>(
			P48PCGSpawnAttributeNames::SurfaceNormal,
			FVector::UpVector,
			false,
			false);
		auto* OutputLocations = Metadata->CreateAttribute<FVector>(
			P48PCGSpawnAttributeNames::SurfaceLocation,
			FVector::ZeroVector,
			false,
			false);
		auto* OutputSlots = Metadata->CreateAttribute<int32>(
			P48PCGSpawnAttributeNames::SpawnSlotIndex,
			INDEX_NONE,
			false,
			false);
		auto* OutputGenerationIds = Metadata->CreateAttribute<int32>(
			P48PCGSpawnAttributeNames::GenerationId,
			0,
			false,
			false);

		TArray<FPCGPoint> Points;
		int32 IslandsWithoutPoint = 0;
		for (int32 IslandPointIndex = 0; IslandPointIndex < Islands->GetNumPoints(); ++IslandPointIndex)
		{
			const PCGMetadataEntryKey Entry = Islands->GetMetadataEntry(IslandPointIndex);
			UStaticMesh* Mesh = Cast<UStaticMesh>(
				MeshAttribute->GetValueFromItemKey(Entry).ResolveObject());
			if (!Mesh)
			{
				++IslandsWithoutPoint;
				continue;
			}

			const int32 IslandId = IslandAttribute->GetValueFromItemKey(Entry);
			const FTransform IslandTransform = Islands->GetTransform(IslandPointIndex);
			const FBox Bounds = Mesh->GetBounds().TransformBy(IslandTransform).GetBox();
			const FVector Center = Bounds.GetCenter();
			const int32 IslandSeed = PCGHelpers::ComputeSeed(NetworkSeed, IslandId);
			FRandomStream Random(IslandSeed);
			const double StartAngle = Random.FRandRange(0.0f, 2.0f * PI);

			FHitResult SelectedHit;
			bool bFoundPoint = P48ItemSpawnPoint::IsSafeSurface(
				World,
				Mesh,
				IslandTransform,
				Bounds,
				Center,
				SafeRadius,
				MinNormalZ,
				SelectedHit);

			for (int32 RingIndex = 1; RingIndex <= RingCount && !bFoundPoint; ++RingIndex)
			{
				const float Radius = FMath::Min(SearchRadius, RingIndex * Step);
				for (int32 SampleIndex = 0; SampleIndex < SamplesPerRing; ++SampleIndex)
				{
					const double Angle = StartAngle + 2.0 * PI * SampleIndex / SamplesPerRing;
					const FVector Candidate = Center + FVector(
						FMath::Cos(Angle) * Radius,
						FMath::Sin(Angle) * Radius,
						0.0f);
					if (P48ItemSpawnPoint::IsSafeSurface(
						World,
						Mesh,
						IslandTransform,
						Bounds,
						Candidate,
						SafeRadius,
						MinNormalZ,
						SelectedHit))
					{
						bFoundPoint = true;
						break;
					}
				}
			}

			if (!bFoundPoint)
			{
				++IslandsWithoutPoint;
				continue;
			}

			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = FTransform(
				FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
				SelectedHit.ImpactPoint);
			Point.Density = 1.0f;
			Point.Seed = IslandSeed;
			Point.BoundsMin = FVector(-25.0f);
			Point.BoundsMax = FVector(25.0f);
			Point.MetadataEntry = Metadata->AddEntry();
			OutputIslandIds->SetValue(Point.MetadataEntry, IslandId);
			OutputNormals->SetValue(Point.MetadataEntry, SelectedHit.ImpactNormal);
			OutputLocations->SetValue(Point.MetadataEntry, SelectedHit.ImpactPoint);
			OutputSlots->SetValue(Point.MetadataEntry, Points.Num() - 1);
			OutputGenerationIds->SetValue(Point.MetadataEntry, GenerationContext.GenerationId);
		}

		Output->SetNumPoints(Points.Num(), false);
		Output->AllocateProperties(EPCGPointNativeProperties::All);
		FPCGPointValueRanges Ranges(Output, false);
		for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
		{
			Ranges.SetFromPoint(PointIndex, Points[PointIndex]);
		}

		FPCGTaggedData& Tagged = Context->OutputData.TaggedData.Emplace_GetRef();
		Tagged.Data = Output;
		Tagged.Pin = PCGPinConstants::DefaultOutputLabel;
		Tagged.Tags = Input.Tags;

		if (Settings->bLogDiagnostics)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[P48ItemSpawnPoint] Islands=%d Missing=%d Points=%d"),
				Islands->GetNumPoints(),
				IslandsWithoutPoint,
				Points.Num());
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
