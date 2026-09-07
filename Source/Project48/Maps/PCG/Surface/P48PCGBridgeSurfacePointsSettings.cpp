#include "P48PCGBridgeSurfacePointsSettings.h"

#include "P48IslandSurfaceSampler.h"
#include "../Common/P48PCGSpawnAttributeNames.h"
#include "Data/PCGPointData.h"
#include "Engine/StaticMesh.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Engine/World.h"
#include "PCGContext.h"
#include "PCGModule.h"

#define LOCTEXT_NAMESPACE "P48BridgeSurfacePoints"

#if WITH_EDITOR
FText UP48PCGBridgeSurfacePointsSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("Title", "P48 Bridge Surface Points");
}

FText UP48PCGBridgeSurfacePointsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Finds precise bridge entrance candidates on spawned static-mesh islands. Connect Static Mesh Spawner Out to In. Outputs IslandIndex, SurfaceNormal and SurfaceOutward attributes.");
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

	FP48PCGBridgeSurfacePointsContext* SurfaceContext = static_cast<FP48PCGBridgeSurfacePointsContext*>(Context);
	if (World->IsGameWorld())
	{
		if (!SurfaceContext->bCollisionWaitStarted)
		{
			SurfaceContext->bCollisionWaitStarted = true;
			SurfaceContext->CollisionWaitStartFrame = GFrameCounter;
		}

		// Static Mesh Spawner가 새 인스턴스를 만든 프레임이 끝난 뒤에만 충돌을 조회합니다.
		// 이 대기가 없으면 새 섬 대신 직전 세대의 Physics State를 읽을 수 있습니다.
		if (GFrameCounter <= SurfaceContext->CollisionWaitStartFrame)
		{
			SurfaceContext->bIsPaused = true;
			FPCGModule::GetPCGModuleChecked().ExecuteNextTick([ContextHandle = SurfaceContext->GetOrCreateHandle()]()
			{
				FPCGContext::FSharedContext<FP48PCGBridgeSurfacePointsContext> SharedContext(ContextHandle);
				if (FP48PCGBridgeSurfacePointsContext* PendingContext = SharedContext.Get())
				{
					PendingContext->bIsPaused = false;
				}
			});
			return false;
		}
	}

	const int32 Directions = FMath::Clamp(Settings->DirectionCount, 4, 64);
	const int32 Steps = FMath::Clamp(Settings->RadialSteps, 4, 64);
	const int32 RefinementSteps = FMath::Clamp(Settings->EdgeRefinementSteps, 3, 12);
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
		auto* Normals = Metadata->CreateAttribute<FVector>(P48PCGSpawnAttributeNames::SurfaceNormal, FVector::UpVector, false, false);
		auto* Outwards = Metadata->CreateAttribute<FVector>(P48PCGSpawnAttributeNames::SurfaceOutward, FVector::ForwardVector, false, false);
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
			FP48IslandSurfaceTraceStats* TraceStats = Settings->bLogDiagnostics ? &Stats : nullptr;
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
				double OutsideDistance = Radius;
				double InsideDistance = 0.0;
				bool bFoundInside = false;
				for (int32 Step = Steps; Step >= 0 && TraceCount < MaxTraces; --Step)
				{
					const double Distance = Radius * Step / Steps;
					FHitResult Hit;
					++TraceCount;
					if (!FP48IslandSurfaceSampler::TraceTop(World, Mesh, Transform, Bounds, Center + Direction * Distance, Hit, TraceStats))
					{
						OutsideDistance = Distance;
						continue;
					}
					if (Hit.ImpactNormal.Z < MinNormalZ)
					{
						++EdgeSlopeRejected;
						OutsideDistance = Distance;
						continue;
					}
					InsideDistance = Distance;
					bFoundInside = true;
					break;
				}
				if (!bFoundInside)
				{
					continue;
				}

				// 바깥의 miss와 안쪽의 hit 사이를 정밀화합니다. 큰 섬에서도 RadialSteps 오차가 누적되지 않습니다.
				for (int32 Refinement = 0; Refinement < RefinementSteps && TraceCount < MaxTraces; ++Refinement)
				{
					const double MidDistance = (OutsideDistance + InsideDistance) * 0.5;
					FHitResult MidHit;
					++TraceCount;
					if (FP48IslandSurfaceSampler::TraceTop(World, Mesh, Transform, Bounds, Center + Direction * MidDistance, MidHit, TraceStats) && MidHit.ImpactNormal.Z >= MinNormalZ)
					{
						InsideDistance = MidDistance;
					}
					else
					{
						OutsideDistance = MidDistance;
					}
				}

				FHitResult Surface;
				++TraceCount;
				const double MaxSafeInset = InsideDistance * 0.25;
				const double SafeInset = FMath::Min<double>(FMath::Max(0.0f, Settings->EdgeInset), MaxSafeInset);
				const FVector Candidate = Center + Direction * (InsideDistance - SafeInset);
				if (!FP48IslandSurfaceSampler::TraceTop(World, Mesh, Transform, Bounds, Candidate, Surface, TraceStats)) { continue; }
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
						Outwards->SetValue(Point.MetadataEntry, Direction);
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
			if (Points.IsEmpty())
			{
				PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoSurface", "No island surface candidates were found. Check spawn order, query collision, slope limit and EdgeInset."));
			}
			else if (Settings->bLogDiagnostics)
			{
				UE_LOG(LogTemp, Display, TEXT("[P48Surface] %d island(s) were skipped because they have no valid walkable edge. Valid islands continue without a graph warning."), EmptyIslands);
			}
		}
	}
	if (TraceCount + 2 > MaxTraces)
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("Budget", "Surface trace limit reached; output is partial. Reduce DirectionCount or RadialSteps."));
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
