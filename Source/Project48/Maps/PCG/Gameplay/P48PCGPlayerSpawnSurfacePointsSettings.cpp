#include "P48PCGPlayerSpawnSurfacePointsSettings.h"

#include "../Common/P48PCGSeedHelpers.h"
#include "../Common/P48PCGSpawnAttributeNames.h"
#include "../Surface/P48IslandSurfaceSampler.h"
#include "Data/PCGPointData.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGContext.h"
#include "PCGModule.h"

#define LOCTEXT_NAMESPACE "P48PlayerSpawnSurfacePoints"

namespace P48PlayerSpawnSurface
{
	constexpr int32 SupportDirectionCount = 8;
	constexpr int32 MaxCandidatesPerIsland = 32;
	constexpr int32 MaxGridSamplesPerAxis = 32;
	constexpr float CapsuleRadius = 42.0f;
	constexpr float CapsuleHalfHeight = 96.0f;
	constexpr float GroundClearance = 5.0f;

	bool HasCapsuleClearance(const UWorld* World, const FVector& SurfaceLocation)
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(P48PlayerSpawnClearance), false);
		const FVector CapsuleCenter = SurfaceLocation + FVector::UpVector * (CapsuleHalfHeight + GroundClearance);
		return !World->OverlapBlockingTestByChannel(
			CapsuleCenter,
			FQuat::Identity,
			ECC_Pawn,
			FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
			QueryParams);
	}
}

#if WITH_EDITOR
FText UP48PCGPlayerSpawnSurfacePointsSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("Title", "P48 Player Spawn Surface Points");
}

FText UP48PCGPlayerSpawnSurfacePointsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip", "Creates safe PlayerStart candidates after the map barrier. Requires Mesh, IslandIndex and CanSpawnPlayer attributes from spawned island points.");
}
#endif

TArray<FPCGPinProperties> UP48PCGPlayerSpawnSurfacePointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	Pins.Emplace(P48PCGSeedNames::InputPin, EPCGDataType::Param);
	return Pins;
}

TArray<FPCGPinProperties> UP48PCGPlayerSpawnSurfacePointsSettings::OutputPinProperties() const
{
	return Super::DefaultPointOutputPinProperties();
}

FPCGElementPtr UP48PCGPlayerSpawnSurfacePointsSettings::CreateElement() const
{
	return MakeShared<FP48PCGPlayerSpawnSurfaceElement>();
}

bool FP48PCGPlayerSpawnSurfaceElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	const auto* Settings = Context->GetInputSettings<UP48PCGPlayerSpawnSurfacePointsSettings>();
	UWorld* World = Context->ExecutionSource.IsValid() ? Context->ExecutionSource->GetExecutionState().GetWorld() : nullptr;
	if (!Settings || !World)
	{
		return true;
	}

	auto* SpawnContext = static_cast<FP48PCGPlayerSpawnSurfaceContext*>(Context);
	if (World->IsGameWorld())
	{
		if (!SpawnContext->bCollisionWaitStarted)
		{
			SpawnContext->bCollisionWaitStarted = true;
			SpawnContext->CollisionWaitStartFrame = GFrameCounter;
		}
		if (GFrameCounter <= SpawnContext->CollisionWaitStartFrame)
		{
			SpawnContext->bIsPaused = true;
			FPCGModule::GetPCGModuleChecked().ExecuteNextTick([ContextHandle = SpawnContext->GetOrCreateHandle()]()
			{
				FPCGContext::FSharedContext<FP48PCGPlayerSpawnSurfaceContext> SharedContext(ContextHandle);
				if (FP48PCGPlayerSpawnSurfaceContext* PendingContext = SharedContext.Get())
				{
					PendingContext->bIsPaused = false;
				}
			});
			return false;
		}
	}

	const int32 NetworkSeed = P48ReadNetworkSeed(Context);
	const float Spacing = FMath::Max(100.0f, Settings->CandidateSpacing);
	const float SafeDistance = FMath::Max(0.0f, Settings->EdgeSafeDistance);
	const double MinNormalZ = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Settings->MaxSurfaceSlope, 0.0f, 80.0f)));

	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
	{
		const UPCGBasePointData* Islands = Cast<UPCGBasePointData>(Input.Data);
		if (!Islands || !Islands->Metadata)
		{
			continue;
		}

		const auto* MeshAttribute = Islands->Metadata->GetConstTypedAttribute<FSoftObjectPath>(P48PCGSpawnAttributeNames::Mesh);
		const auto* IslandAttribute = Islands->Metadata->GetConstTypedAttribute<int32>(P48PCGSpawnAttributeNames::IslandIndex);
		const auto* CanSpawnAttribute = Islands->Metadata->GetConstTypedAttribute<bool>(P48PCGSpawnAttributeNames::CanSpawnPlayer);
		if (!MeshAttribute || !IslandAttribute || !CanSpawnAttribute)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingAttributes", "Player Spawn Surface Points requires Mesh, IslandIndex and CanSpawnPlayer attributes from the island Static Mesh Spawner output."));
			continue;
		}

		UPCGBasePointData* Output = FPCGContext::NewPointData_AnyThread(Context);
		UPCGMetadata* Metadata = Output->MutableMetadata();
		auto* OutputIslandIds = Metadata->CreateAttribute<int32>(P48PCGSpawnAttributeNames::IslandIndex, INDEX_NONE, false, false);
		auto* OutputNormals = Metadata->CreateAttribute<FVector>(P48PCGSpawnAttributeNames::SurfaceNormal, FVector::UpVector, false, false);
		auto* OutputLocations = Metadata->CreateAttribute<FVector>(P48PCGSpawnAttributeNames::SurfaceLocation, FVector::ZeroVector, false, false);
		TArray<FPCGPoint> Points;
		int32 EligibleIslandCount = 0;
		int32 IslandsWithoutCandidates = 0;

		for (int32 IslandPointIndex = 0; IslandPointIndex < Islands->GetNumPoints(); ++IslandPointIndex)
		{
			const PCGMetadataEntryKey Entry = Islands->GetMetadataEntry(IslandPointIndex);
			if (!CanSpawnAttribute->GetValueFromItemKey(Entry))
			{
				continue;
			}
			++EligibleIslandCount;

			UStaticMesh* Mesh = Cast<UStaticMesh>(MeshAttribute->GetValueFromItemKey(Entry).ResolveObject());
			if (!Mesh)
			{
				++IslandsWithoutCandidates;
				continue;
			}

			const int32 IslandId = IslandAttribute->GetValueFromItemKey(Entry);
			const FTransform IslandTransform = Islands->GetTransform(IslandPointIndex);
			const FBox Bounds = Mesh->GetBounds().TransformBy(IslandTransform).GetBox();
			const FVector BoundsSize = Bounds.GetSize();
			const int32 SamplesX = FMath::Clamp(FMath::CeilToInt(BoundsSize.X / Spacing), 1, P48PlayerSpawnSurface::MaxGridSamplesPerAxis);
			const int32 SamplesY = FMath::Clamp(FMath::CeilToInt(BoundsSize.Y / Spacing), 1, P48PlayerSpawnSurface::MaxGridSamplesPerAxis);
			int32 AcceptedForIsland = 0;

			for (int32 Y = 0; Y < SamplesY && AcceptedForIsland < P48PlayerSpawnSurface::MaxCandidatesPerIsland; ++Y)
			{
				for (int32 X = 0; X < SamplesX && AcceptedForIsland < P48PlayerSpawnSurface::MaxCandidatesPerIsland; ++X)
				{
					const FVector SampleLocation(
						FMath::Lerp(Bounds.Min.X, Bounds.Max.X, (X + 0.5) / SamplesX),
						FMath::Lerp(Bounds.Min.Y, Bounds.Max.Y, (Y + 0.5) / SamplesY),
						Bounds.GetCenter().Z);
					FHitResult SurfaceHit;
					if (!FP48IslandSurfaceSampler::TraceTop(World, Mesh, IslandTransform, Bounds, SampleLocation, SurfaceHit, nullptr, true) || SurfaceHit.ImpactNormal.Z < MinNormalZ)
					{
						continue;
					}

					bool bSafeEdge = true;
					for (int32 DirectionIndex = 0; DirectionIndex < P48PlayerSpawnSurface::SupportDirectionCount && bSafeEdge && SafeDistance > 0.0f; ++DirectionIndex)
					{
						const double Angle = 2.0 * PI * DirectionIndex / P48PlayerSpawnSurface::SupportDirectionCount;
						const FVector Offset(FMath::Cos(Angle) * SafeDistance, FMath::Sin(Angle) * SafeDistance, 0.0f);
						FHitResult SupportHit;
						bSafeEdge = FP48IslandSurfaceSampler::TraceTop(World, Mesh, IslandTransform, Bounds, SurfaceHit.ImpactPoint + Offset, SupportHit, nullptr, true) &&
							SupportHit.ImpactNormal.Z >= MinNormalZ &&
							FMath::Abs(SupportHit.ImpactPoint.Z - SurfaceHit.ImpactPoint.Z) <= FMath::Max(100.0f, SafeDistance * 0.25f);
					}
					if (!bSafeEdge || !P48PlayerSpawnSurface::HasCapsuleClearance(World, SurfaceHit.ImpactPoint))
					{
						continue;
					}

					const FVector ToIslandCenter = FVector(Bounds.GetCenter().X - SurfaceHit.ImpactPoint.X, Bounds.GetCenter().Y - SurfaceHit.ImpactPoint.Y, 0.0f).GetSafeNormal();
					FPCGPoint& Point = Points.Emplace_GetRef();
					Point.Transform = FTransform(ToIslandCenter.Rotation(), SurfaceHit.ImpactPoint);
					Point.Density = 1.0f;
					Point.Seed = PCGHelpers::ComputeSeed(
						NetworkSeed,
						HashCombine(Islands->GetSeed(IslandPointIndex), HashCombine(X, Y)));
					Point.BoundsMin = FVector(-25.0f);
					Point.BoundsMax = FVector(25.0f);
					Point.MetadataEntry = Metadata->AddEntry();
					OutputIslandIds->SetValue(Point.MetadataEntry, IslandId);
					OutputNormals->SetValue(Point.MetadataEntry, SurfaceHit.ImpactNormal);
					OutputLocations->SetValue(Point.MetadataEntry, SurfaceHit.ImpactPoint);
					++AcceptedForIsland;
				}
			}

			if (AcceptedForIsland == 0)
			{
				++IslandsWithoutCandidates;
			}
		}

		Output->SetNumPoints(Points.Num(), false);
		Output->AllocateProperties(EPCGPointNativeProperties::All);
		FPCGPointValueRanges Ranges(Output, false);
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			Ranges.SetFromPoint(Index, Points[Index]);
		}

		FPCGTaggedData& Tagged = Context->OutputData.TaggedData.Emplace_GetRef();
		Tagged.Data = Output;
		Tagged.Pin = PCGPinConstants::DefaultOutputLabel;
		Tagged.Tags = Input.Tags;

		if (Points.IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoCandidates", "No safe PlayerStart surface candidates were found."));
		}
		else if (Settings->bLogDiagnostics)
		{
			UE_LOG(LogTemp, Display, TEXT("[P48PlayerSpawnSurface] EligibleIslands=%d IslandsWithoutCandidates=%d Candidates=%d"), EligibleIslandCount, IslandsWithoutCandidates, Points.Num());
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
