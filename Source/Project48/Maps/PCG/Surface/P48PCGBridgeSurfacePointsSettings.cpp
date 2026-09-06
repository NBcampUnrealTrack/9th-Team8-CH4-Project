#include "P48PCGBridgeSurfacePointsSettings.h"

#include "P48IslandSurfaceSampler.h"
#include "../Common/P48PCGSpawnAttributeNames.h"
#include "Data/PCGPointData.h"
#include "Engine/StaticMesh.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGComponent.h"
#include "PCGContext.h"

#define LOCTEXT_NAMESPACE "P48BridgeSurfacePoints"

#if WITH_EDITOR
FText UP48PCGBridgeSurfacePointsSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("Title", "P48 Bridge Surface Points");
}

FText UP48PCGBridgeSurfacePointsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Debug-only bridge entrance candidates on spawned static-mesh islands. Connect Static Mesh Spawner Out to In. Requires query collision and Mesh / IslandIndex attributes. Does not spawn bridges or choose MST edges.");
}
#endif

TArray<FPCGPinProperties> UP48PCGBridgeSurfacePointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	return Pins;
}

TArray<FPCGPinProperties> UP48PCGBridgeSurfacePointsSettings::OutputPinProperties() const
{
	return { FPCGPinProperties(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point) };
}

FPCGElementPtr UP48PCGBridgeSurfacePointsSettings::CreateElement() const
{
	return MakeShared<FP48PCGBridgeSurfacePointsElement>();
}

bool FP48PCGBridgeSurfacePointsElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	const auto* Settings = Context->GetInputSettings<UP48PCGBridgeSurfacePointsSettings>();
	UWorld* World = Context->ExecutionSource.IsValid() ? Context->ExecutionSource->GetExecutionState().GetWorld() : nullptr;
	if (!Settings || !World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[P48Surface] Execution aborted: Settings=%d World=%s"), Settings != nullptr, *GetNameSafe(World));
		return true;
	}
	const int32 Directions = FMath::Clamp(Settings->DirectionCount, 4, 64);
	const int32 Steps = FMath::Clamp(Settings->RadialSteps, 4, 64);
	const double MinNormalZ = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Settings->MaxSurfaceSlope, 0.0f, 80.0f)));
	int32 TraceCount = 0;
	constexpr int32 MaxTraces = 32768;
	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
	{
		const UPCGBasePointData* Islands = Cast<UPCGBasePointData>(Input.Data);
		if (!Islands || !Islands->Metadata) { continue; }
		const auto* MeshAttribute = Islands->Metadata->GetConstTypedAttribute<FSoftObjectPath>(P48PCGSpawnAttributeNames::Mesh);
		const auto* IslandAttribute = Islands->Metadata->GetConstTypedAttribute<int32>(P48PCGSpawnAttributeNames::IslandIndex);
		if (!MeshAttribute || !IslandAttribute)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingAttributes", "Connect island Static Mesh Spawner Out. Mesh and IslandIndex attributes from Air Structure Generator are required; Connection Anchors are not surface input."));
			continue;
		}
		UPCGBasePointData* Output = FPCGContext::NewPointData_AnyThread(Context);
		UPCGMetadata* Metadata = Output->MutableMetadata();
		auto* Ids = Metadata->CreateAttribute<int32>(P48PCGSpawnAttributeNames::IslandIndex, INDEX_NONE, false, false);
		auto* Normals = Metadata->CreateAttribute<FVector>(TEXT("SurfaceNormal"), FVector::UpVector, false, false);
		TArray<FPCGPoint> Points;
		int32 EmptyIslands = 0;
		for (int32 Island = 0; Island < Islands->GetNumPoints() && TraceCount < MaxTraces; ++Island)
		{
			const PCGMetadataEntryKey Entry = Islands->GetMetadataEntry(Island);
			UStaticMesh* Mesh = Cast<UStaticMesh>(MeshAttribute->GetValueFromItemKey(Entry).ResolveObject());
			if (!Mesh)
			{
				++EmptyIslands;
				if (Settings->bLogDiagnostics) { UE_LOG(LogTemp, Warning, TEXT("[P48Surface] Island=%d MeshResolveFailed Path=%s"), IslandAttribute->GetValueFromItemKey(Entry), *MeshAttribute->GetValueFromItemKey(Entry).ToString()); }
				continue;
			}
			FP48IslandSurfaceTraceStats Stats;
			int32 EdgeSlopeRejected = 0;
			int32 InsetSlopeRejected = 0;
			const FTransform Transform = Islands->GetTransform(Island);
			const FBox Bounds = Mesh->GetBounds().TransformBy(Transform).GetBox();
			const FVector Center = Bounds.GetCenter();
			const double Radius = FVector2D(Bounds.GetExtent().X, Bounds.GetExtent().Y).Length();
			TArray<FVector> Accepted;
			for (int32 DirectionIndex = 0; DirectionIndex < Directions && TraceCount < MaxTraces; ++DirectionIndex)
			{
				const double Angle = 2.0 * PI * DirectionIndex / Directions;
				const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
				for (int32 Step = Steps; Step >= 1 && TraceCount + 2 <= MaxTraces; --Step)
				{
					const double Distance = Radius * Step / Steps;
					FHitResult Edge;
					++TraceCount;
					if (!FP48IslandSurfaceSampler::TraceTop(World, Mesh, Transform, Bounds, Center + Direction * Distance, Edge, &Stats)) { continue; }
					if (Edge.ImpactNormal.Z < MinNormalZ) { ++EdgeSlopeRejected; continue; }
					FHitResult Surface;
					++TraceCount;
					const FVector Candidate = Center + Direction * FMath::Max(0.0, Distance - FMath::Max(0.0f, Settings->EdgeInset));
					if (!FP48IslandSurfaceSampler::TraceTop(World, Mesh, Transform, Bounds, Candidate, Surface, &Stats)) { continue; }
					if (Surface.ImpactNormal.Z < MinNormalZ) { ++InsetSlopeRejected; continue; }
					if (!Accepted.ContainsByPredicate([&Surface](const FVector& P) { return P.Equals(Surface.ImpactPoint, 1.0); }))
					{
						Accepted.Add(Surface.ImpactPoint);
						FPCGPoint& Point = Points.Emplace_GetRef();
						Point.Transform = FTransform(FRotator(0.0, FMath::RadiansToDegrees(Angle), 0.0), Surface.ImpactPoint);
						Point.Density = 1.0f;
						Point.Seed = Islands->GetSeed(Island) ^ DirectionIndex;
						Point.BoundsMin = FVector(-FMath::Max(1.0f, Settings->PointExtent));
						Point.BoundsMax = -Point.BoundsMin;
						Point.MetadataEntry = Metadata->AddEntry();
						Ids->SetValue(Point.MetadataEntry, IslandAttribute->GetValueFromItemKey(Entry));
						Normals->SetValue(Point.MetadataEntry, Surface.ImpactNormal);
					}
					break;
				}
			}
			if (Accepted.IsEmpty()) { ++EmptyIslands; }
			if (Settings->bLogDiagnostics)
			{
				UE_LOG(LogTemp, Display, TEXT("[P48Surface] World=%s Island=%d Mesh=%s Traces=%d NoHitTraces=%d TotalHits=%d MeshMismatchHits=%d InvalidInstanceHits=%d TransformMismatchHits=%d MatchedTraces=%d EdgeSlopeRejected=%d InsetSlopeRejected=%d Points=%d"), *World->GetPathName(), IslandAttribute->GetValueFromItemKey(Entry), *Mesh->GetPathName(), Stats.Traces, Stats.NoHits, Stats.Hits, Stats.MeshMismatchHits, Stats.InvalidInstanceHits, Stats.TransformMismatchHits, Stats.MatchedTraces, EdgeSlopeRejected, InsetSlopeRejected, Accepted.Num());
				if (!Stats.FirstMismatch.IsEmpty()) { UE_LOG(LogTemp, Display, TEXT("[P48Surface] FirstTransformMismatch: %s"), *Stats.FirstMismatch.Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" "))); }
			}
		}
		Output->SetNumPoints(Points.Num(), false);
		Output->AllocateProperties(EPCGPointNativeProperties::All);
		FPCGPointValueRanges Ranges(Output, false);
		for (int32 Index = 0; Index < Points.Num(); ++Index) { Ranges.SetFromPoint(Index, Points[Index]); }
		FPCGTaggedData& Tagged = Context->OutputData.TaggedData.Emplace_GetRef();
		Tagged.Data = Output;
		Tagged.Pin = PCGPinConstants::DefaultOutputLabel;
		Tagged.Tags = Input.Tags;
		if (EmptyIslands > 0)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoSurface", "Some islands have no surface candidates. Check spawn order, query collision (WorldStatic/WorldDynamic), slope limit and EdgeInset. Visual displacement is not collision geometry."));
		}
	}
	if (TraceCount + 2 > MaxTraces)
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("Budget", "Surface trace limit reached; output is partial. Reduce DirectionCount or RadialSteps."));
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
