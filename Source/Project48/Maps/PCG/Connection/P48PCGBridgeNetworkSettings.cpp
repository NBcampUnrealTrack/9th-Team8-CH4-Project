#include "P48PCGBridgeNetworkSettings.h"
#include "../Common/P48PCGSeedHelpers.h"
#include "../../Utilities/P48BridgeConnectionPolicy.h"

#include "../Common/P48PCGSpawnAttributeNames.h"
#include "Data/PCGPointData.h"
#include "Engine/StaticMesh.h"
#include "Helpers/PCGHelpers.h"
#include "Math/RotationMatrix.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGContext.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "P48PCGBridgeNetworkSettings"

const FName UP48PCGBridgeNetworkSettings::StaticMeshOutputLabel(TEXT("BridgeStaticMeshPoints"));
const FName UP48PCGBridgeNetworkSettings::ActorOutputLabel(TEXT("BridgeActorPoints"));

namespace P48BridgeNetwork
{
	struct FSurfaceCandidate
	{
		FVector Location = FVector::ZeroVector;
		FVector Outward = FVector::ForwardVector;
	};

	struct FAnchor
	{
		FVector Location = FVector::ZeroVector;
		float Radius = 0.0f;
		int32 IslandIndex = INDEX_NONE;
		TArray<FSurfaceCandidate> SurfaceCandidates;
	};

	struct FEdge
	{
		int32 A = INDEX_NONE;
		int32 B = INDEX_NONE;
		float Length = 0.0f;
		TArray<FP48BridgeEndpointCandidate> Candidates;
	};

	struct FSelectedConnection
	{
		int32 EdgeIndex = INDEX_NONE;
		int32 CandidateIndex = INDEX_NONE;
	};

	struct FBridgePoint
	{
		FTransform Transform = FTransform::Identity;
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		TObjectPtr<UStaticMesh> Mesh = nullptr;
		TSubclassOf<AActor> ActorClass;
		int32 Seed = 0;
	};

	class FDisjointSet
	{
	public:
		explicit FDisjointSet(const int32 Count)
		{
			Parent.SetNumUninitialized(Count);
			Rank.Init(0, Count);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Parent[Index] = Index;
			}
		}

		int32 Find(const int32 Value)
		{
			if (Parent[Value] != Value)
			{
				Parent[Value] = Find(Parent[Value]);
			}
			return Parent[Value];
		}

		bool Union(const int32 A, const int32 B)
		{
			int32 RootA = Find(A);
			int32 RootB = Find(B);
			if (RootA == RootB)
			{
				return false;
			}

			if (Rank[RootA] < Rank[RootB])
			{
				Swap(RootA, RootB);
			}
			Parent[RootB] = RootA;
			if (Rank[RootA] == Rank[RootB])
			{
				++Rank[RootA];
			}
			return true;
		}

	private:
		TArray<int32> Parent;
		TArray<uint8> Rank;
	};

	constexpr int32 MaxRequiredSelectionSearchStates = 20000;

	bool CanStillConnectAllIslands(
		const TArray<FEdge>& Edges,
		const int32 NextEdgeIndex,
		const int32 IslandCount,
		FDisjointSet Sets)
	{
		for (int32 EdgeIndex = NextEdgeIndex; EdgeIndex < Edges.Num(); ++EdgeIndex)
		{
			Sets.Union(Edges[EdgeIndex].A, Edges[EdgeIndex].B);
		}
		const int32 Root = Sets.Find(0);
		for (int32 IslandIndex = 1; IslandIndex < IslandCount; ++IslandIndex)
		{
			if (Sets.Find(IslandIndex) != Root)
			{
				return false;
			}
		}
		return true;
	}

	bool SearchRequiredConnections(
		const TArray<FEdge>& Edges,
		const TArray<FAnchor>& Anchors,
		const float EndpointExclusionRadius,
		const int32 EdgeIndex,
		FDisjointSet Sets,
		TArray<FSelectedConnection> Selections,
		TMap<int32, TArray<FVector>> UsedEndpointsByIsland,
		int32& SearchStateCount,
		bool& bSearchBudgetExhausted,
		TArray<FSelectedConnection>& OutSelections,
		TMap<int32, TArray<FVector>>& OutUsedEndpointsByIsland)
	{
		if (++SearchStateCount > MaxRequiredSelectionSearchStates)
		{
			bSearchBudgetExhausted = true;
			return false;
		}
		if (Selections.Num() == Anchors.Num() - 1)
		{
			OutSelections = MoveTemp(Selections);
			OutUsedEndpointsByIsland = MoveTemp(UsedEndpointsByIsland);
			return true;
		}
		if (EdgeIndex >= Edges.Num() ||
			Selections.Num() + (Edges.Num() - EdgeIndex) < Anchors.Num() - 1 ||
			!CanStillConnectAllIslands(Edges, EdgeIndex, Anchors.Num(), Sets))
		{
			return false;
		}

		const FEdge& Edge = Edges[EdgeIndex];
		if (Sets.Find(Edge.A) != Sets.Find(Edge.B))
		{
			for (int32 CandidateIndex = 0; CandidateIndex < Edge.Candidates.Num(); ++CandidateIndex)
			{
				const FP48BridgeEndpointCandidate& Candidate = Edge.Candidates[CandidateIndex];
				if (!P48BridgeConnectionPolicy::IsCandidateAvailable(
					Candidate,
					Anchors[Edge.A].IslandIndex,
					Anchors[Edge.B].IslandIndex,
					UsedEndpointsByIsland,
					EndpointExclusionRadius))
				{
					continue;
				}

				FDisjointSet CandidateSets = Sets;
				CandidateSets.Union(Edge.A, Edge.B);
				TArray<FSelectedConnection> CandidateSelections = Selections;
				FSelectedConnection& Selection = CandidateSelections.Emplace_GetRef();
				Selection.EdgeIndex = EdgeIndex;
				Selection.CandidateIndex = CandidateIndex;
				TMap<int32, TArray<FVector>> CandidateUsedEndpoints = UsedEndpointsByIsland;
				P48BridgeConnectionPolicy::ReserveCandidateEndpoints(
					Candidate,
					Anchors[Edge.A].IslandIndex,
					Anchors[Edge.B].IslandIndex,
					CandidateUsedEndpoints);

				if (SearchRequiredConnections(
					Edges,
					Anchors,
					EndpointExclusionRadius,
					EdgeIndex + 1,
					MoveTemp(CandidateSets),
					MoveTemp(CandidateSelections),
					MoveTemp(CandidateUsedEndpoints),
					SearchStateCount,
					bSearchBudgetExhausted,
					OutSelections,
					OutUsedEndpointsByIsland))
				{
					return true;
				}
				if (bSearchBudgetExhausted)
				{
					return false;
				}
			}
		}

		return SearchRequiredConnections(
			Edges,
			Anchors,
			EndpointExclusionRadius,
			EdgeIndex + 1,
			MoveTemp(Sets),
			MoveTemp(Selections),
			MoveTemp(UsedEndpointsByIsland),
			SearchStateCount,
			bSearchBudgetExhausted,
			OutSelections,
			OutUsedEndpointsByIsland);
	}

	bool FacesBridge(const FSurfaceCandidate& Candidate, const FVector& OtherLocation, const FP48BridgeConnectionRules& Rules)
	{
		const FVector ToOther = FVector(OtherLocation.X - Candidate.Location.X, OtherLocation.Y - Candidate.Location.Y, 0.0f).GetSafeNormal();
		const FVector Outward = FVector(Candidate.Outward.X, Candidate.Outward.Y, 0.0f).GetSafeNormal();
		const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Rules.MaxEndpointFacingAngle, 0.0f, 89.0f)));
		return !ToOther.IsNearlyZero() && !Outward.IsNearlyZero() && FVector::DotProduct(ToOther, Outward) >= MinimumDot;
	}

	template <typename TEntry, typename TIsValid>
	const TEntry* SelectWeighted(const TArray<TEntry>& Entries, FRandomStream& Random, TIsValid&& IsValid)
	{
		float TotalWeight = 0.0f;
		for (const TEntry& Entry : Entries)
		{
			TotalWeight += IsValid(Entry) ? FMath::Max(0.0f, Entry.Weight) : 0.0f;
		}
		if (TotalWeight <= 0.0f)
		{
			return nullptr;
		}

		const float Selection = Random.FRandRange(0.0f, TotalWeight);
		float Accumulated = 0.0f;
		for (const TEntry& Entry : Entries)
		{
			if (!IsValid(Entry))
			{
				continue;
			}
			Accumulated += FMath::Max(0.0f, Entry.Weight);
			if (Selection <= Accumulated)
			{
				return &Entry;
			}
		}
		return nullptr;
	}

	void CalculateEndpoints(const FAnchor& A, const FAnchor& B, const float Inset, FVector& OutStart, FVector& OutEnd)
	{
		const FVector Delta = B.Location - A.Location;
		const FVector2D HorizontalDelta(Delta.X, Delta.Y);
		const FVector2D HorizontalDirection = HorizontalDelta.GetSafeNormal();
		const FVector Direction(HorizontalDirection.X, HorizontalDirection.Y, 0.0f);
		OutStart = A.Location + Direction * FMath::Max(0.0f, A.Radius - Inset);
		OutEnd = B.Location - Direction * FMath::Max(0.0f, B.Radius - Inset);
	}

	UPCGBasePointData* CreateOutputData(FPCGContext* Context, const TArray<FBridgePoint>& Bridges, const bool bStaticMesh)
	{
		const UWorld* World = Context->ExecutionSource.IsValid() ? Context->ExecutionSource->GetExecutionState().GetWorld() : nullptr;
		const bool bClient = World && World->GetNetMode() == NM_Client;
		TArray<const FBridgePoint*> Matching;
		for (const FBridgePoint& Bridge : Bridges)
		{
			if ((bStaticMesh && Bridge.Mesh) || (!bStaticMesh && Bridge.ActorClass && !bClient))
			{
				Matching.Add(&Bridge);
			}
		}

		UPCGBasePointData* PointData = FPCGContext::NewPointData_AnyThread(Context);
		PointData->SetNumPoints(Matching.Num(), false);
		PointData->AllocateProperties(EPCGPointNativeProperties::All);
		UPCGMetadata* Metadata = PointData->MutableMetadata();
		FPCGMetadataAttribute<FSoftObjectPath>* MeshAttribute = bStaticMesh ? Metadata->CreateAttribute<FSoftObjectPath>(P48PCGSpawnAttributeNames::Mesh, FSoftObjectPath(), false, false) : nullptr;
		FPCGMetadataAttribute<FSoftClassPath>* ActorClassAttribute = !bStaticMesh ? Metadata->CreateAttribute<FSoftClassPath>(P48PCGSpawnAttributeNames::ActorClass, FSoftClassPath(), false, false) : nullptr;
		FPCGMetadataAttribute<FVector>* StartAttribute = Metadata->CreateAttribute<FVector>(P48PCGSpawnAttributeNames::StartLocation, FVector::ZeroVector, false, false);
		FPCGMetadataAttribute<FVector>* EndAttribute = Metadata->CreateAttribute<FVector>(P48PCGSpawnAttributeNames::EndLocation, FVector::ZeroVector, false, false);

		FPCGPointValueRanges Ranges(PointData, false);
		for (int32 Index = 0; Index < Matching.Num(); ++Index)
		{
			const FBridgePoint& Bridge = *Matching[Index];
			FPCGPoint Point;
			Point.Transform = Bridge.Transform;
			Point.Density = 1.0f;
			Point.Seed = Bridge.Seed;
			Point.MetadataEntry = Metadata->AddEntry();
			if (bStaticMesh)
			{
				const FBoxSphereBounds Bounds = Bridge.Mesh->GetBounds();
				Point.BoundsMin = Bounds.Origin - Bounds.BoxExtent;
				Point.BoundsMax = Bounds.Origin + Bounds.BoxExtent;
			}
			if (MeshAttribute)
			{
				MeshAttribute->SetValue(Point.MetadataEntry, FSoftObjectPath(Bridge.Mesh));
			}
			if (ActorClassAttribute)
			{
				ActorClassAttribute->SetValue(Point.MetadataEntry, FSoftClassPath(Bridge.ActorClass.Get()));
			}
			StartAttribute->SetValue(Point.MetadataEntry, Bridge.Start);
			EndAttribute->SetValue(Point.MetadataEntry, Bridge.End);
			Ranges.SetFromPoint(Index, Point);
		}
		return PointData;
	}
}

#if WITH_EDITOR
FText UP48PCGBridgeNetworkSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "P48 Bridge Network Generator");
}

FText UP48PCGBridgeNetworkSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Accepts Bridge Surface Points (IslandIndex + SurfaceNormal + SurfaceOutward) or legacy Connection Anchors. Surface candidates must face the other island before entering the MST. Surface positions are used unchanged; AnchorInset applies only to legacy anchors.");
}
#endif

TArray<FPCGPinProperties> UP48PCGBridgeNetworkSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	FPCGPinProperties& Input = Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
	Input.SetRequiredPin();
	Pins.Emplace(P48PCGSeedNames::InputPin, EPCGDataType::Param);
	return Pins;
}

TArray<FPCGPinProperties> UP48PCGBridgeNetworkSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(StaticMeshOutputLabel, EPCGDataType::Point);
	Pins.Emplace(ActorOutputLabel, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UP48PCGBridgeNetworkSettings::CreateElement() const
{
	return MakeShared<FP48PCGBridgeNetworkElement>();
}

bool FP48PCGBridgeNetworkElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	const UP48PCGBridgeNetworkSettings* Settings = Context->GetInputSettings<UP48PCGBridgeNetworkSettings>();
	if (!Settings)
	{
		return true;
	}

	TArray<P48BridgeNetwork::FBridgePoint> BridgePoints;
	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGBasePointData* InputPoints = Cast<UPCGBasePointData>(Input.Data);
		if (!InputPoints || InputPoints->GetNumPoints() < 2 || !InputPoints->Metadata)
		{
			continue;
		}

		const FPCGMetadataAttribute<float>* RadiusAttribute = InputPoints->Metadata->GetConstTypedAttribute<float>(P48PCGSpawnAttributeNames::PlacementRadius);
		const FPCGMetadataAttribute<int32>* IslandIndexAttribute = InputPoints->Metadata->GetConstTypedAttribute<int32>(P48PCGSpawnAttributeNames::IslandIndex);
		const bool bSurfaceInput = InputPoints->Metadata->GetConstTypedAttribute<FVector>(P48PCGSpawnAttributeNames::SurfaceNormal) != nullptr;
		const FPCGMetadataAttribute<FVector>* SurfaceOutwardAttribute = InputPoints->Metadata->GetConstTypedAttribute<FVector>(P48PCGSpawnAttributeNames::SurfaceOutward);
		if (bSurfaceInput && !IslandIndexAttribute)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingIslandId", "Surface points require IslandIndex to group points by island."));
			continue;
		}
		if (bSurfaceInput && !SurfaceOutwardAttribute)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingSurfaceOutward", "Surface points require SurfaceOutward. Reconnect or regenerate the P48 Bridge Surface Points node."));
			continue;
		}
		if (!bSurfaceInput && !RadiusAttribute)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingRadius", "Input does not contain the PlacementRadius attribute from P48 Air Structure Generator."));
			continue;
		}

		TArray<P48BridgeNetwork::FAnchor> Anchors;
		Anchors.Reserve(InputPoints->GetNumPoints());
		TMap<int32, int32> IslandGroups;
		for (int32 Index = 0; Index < InputPoints->GetNumPoints(); ++Index)
		{
			const PCGMetadataEntryKey Entry = InputPoints->GetMetadataEntry(Index);
			if (bSurfaceInput)
			{
				const int32 IslandId = IslandIndexAttribute->GetValueFromItemKey(Entry);
				if (IslandId == INDEX_NONE) { continue; }
				int32* Group = IslandGroups.Find(IslandId);
				if (!Group)
				{
					const int32 NewIndex = Anchors.AddDefaulted();
					Anchors[NewIndex].IslandIndex = IslandId;
					Group = &IslandGroups.Add(IslandId, NewIndex);
				}
				P48BridgeNetwork::FSurfaceCandidate& Candidate = Anchors[*Group].SurfaceCandidates.Emplace_GetRef();
				Candidate.Location = InputPoints->GetTransform(Index).GetLocation();
				Candidate.Outward = SurfaceOutwardAttribute->GetValueFromItemKey(Entry);
				continue;
			}
			P48BridgeNetwork::FAnchor& Anchor = Anchors.Emplace_GetRef();
			Anchor.Location = InputPoints->GetTransform(Index).GetLocation();
			Anchor.Radius = FMath::Max(0.0f, RadiusAttribute->GetValueFromItemKey(Entry));
			Anchor.IslandIndex = IslandIndexAttribute ? IslandIndexAttribute->GetValueFromItemKey(Entry) : Index;
		}

		if (Anchors.Num() < 2)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NotEnoughIslands", "At least two distinct islands with valid candidates are required. No bridge spawned."));
			continue;
		}
		TArray<P48BridgeNetwork::FEdge> ValidEdges;
		for (int32 A = 0; A < Anchors.Num(); ++A)
		{
			for (int32 B = A + 1; B < Anchors.Num(); ++B)
			{
				P48BridgeNetwork::FEdge Edge;
				Edge.A = A;
				Edge.B = B;
				if (bSurfaceInput)
				{
					// 섬 쌍의 모든 유효 후보를 보관한 뒤 높은 공통 Z부터 대체할 수 있게 정렬합니다.
					for (const P48BridgeNetwork::FSurfaceCandidate& StartCandidate : Anchors[A].SurfaceCandidates)
					{
						for (const P48BridgeNetwork::FSurfaceCandidate& EndCandidate : Anchors[B].SurfaceCandidates)
						{
							if (!P48BridgeNetwork::FacesBridge(StartCandidate, EndCandidate.Location, Settings->GenerationSettings.ConnectionRules) ||
								!P48BridgeNetwork::FacesBridge(EndCandidate, StartCandidate.Location, Settings->GenerationSettings.ConnectionRules))
							{
								continue;
							}
							float CandidateLength = 0.0f;
							if (P48BridgeConnectionPolicy::IsGeometryValid(StartCandidate.Location, EndCandidate.Location, Settings->GenerationSettings.ConnectionRules, CandidateLength))
							{
								FP48BridgeEndpointCandidate& Candidate = Edge.Candidates.Emplace_GetRef();
								Candidate.Start = StartCandidate.Location;
								Candidate.End = EndCandidate.Location;
								Candidate.Length = CandidateLength;
							}
						}
					}
					if (!Edge.Candidates.IsEmpty())
					{
						P48BridgeConnectionPolicy::SortEndpointCandidates(Edge.Candidates);
						Edge.Length = TNumericLimits<float>::Max();
						for (const FP48BridgeEndpointCandidate& Candidate : Edge.Candidates)
						{
							Edge.Length = FMath::Min(Edge.Length, Candidate.Length);
						}
						ValidEdges.Add(MoveTemp(Edge));
					}
					continue;
				}
				float Length = 0.0f;
				if (P48BridgeConnectionPolicy::IsGeometryValid(Anchors[A].Location, Anchors[B].Location, Settings->GenerationSettings.ConnectionRules, Length))
				{
					FP48BridgeEndpointCandidate& Candidate = Edge.Candidates.Emplace_GetRef();
					P48BridgeNetwork::CalculateEndpoints(Anchors[A], Anchors[B], Settings->GenerationSettings.AnchorInset, Candidate.Start, Candidate.End);
					Candidate.Length = FVector::Distance(Candidate.Start, Candidate.End);
					Edge.Length = Length;
					ValidEdges.Add(MoveTemp(Edge));
				}
			}
		}

		ValidEdges.Sort([](const P48BridgeNetwork::FEdge& Left, const P48BridgeNetwork::FEdge& Right)
		{
			if (Left.Length != Right.Length) { return Left.Length < Right.Length; }
			if (Left.A != Right.A) { return Left.A < Right.A; }
			return Left.B < Right.B;
		});
		TArray<P48BridgeNetwork::FSelectedConnection> SelectedConnections;
		TMap<int32, TArray<FVector>> UsedEndpointsByIsland;
		int32 SearchStateCount = 0;
		bool bSearchBudgetExhausted = false;
		if (!P48BridgeNetwork::SearchRequiredConnections(
			ValidEdges,
			Anchors,
			Settings->GenerationSettings.EndpointExclusionRadius,
			0,
			P48BridgeNetwork::FDisjointSet(Anchors.Num()),
			{},
			{},
			SearchStateCount,
			bSearchBudgetExhausted,
			SelectedConnections,
			UsedEndpointsByIsland))
		{
			if (bSearchBudgetExhausted)
			{
				PCGE_LOG(Error, GraphAndLog, LOCTEXT("SelectionBudgetExhausted", "Bridge endpoint selection exceeded its safety search budget. No occupied endpoint was reused and no bridges were emitted for this input."));
			}
			else
			{
				PCGE_LOG(Error, GraphAndLog, LOCTEXT("DisconnectedGraph", "The bridge rules, endpoint-facing test, and endpoint exclusion radius cannot connect every island without reusing an occupied endpoint. No bridges were emitted for this input."));
			}
			continue;
		}
		TSet<int32> SelectedEdgeSet;
		for (const P48BridgeNetwork::FSelectedConnection& Selection : SelectedConnections)
		{
			SelectedEdgeSet.Add(Selection.EdgeIndex);
		}

		FRandomStream Random(PCGHelpers::ComputeSeed(Settings->GenerationSettings.RandomSeed, P48ReadNetworkSeed(Context)));
		for (int32 EdgeIndex = 0; EdgeIndex < ValidEdges.Num(); ++EdgeIndex)
		{
			if (SelectedEdgeSet.Contains(EdgeIndex) || Random.FRand() > Settings->GenerationSettings.AdditionalBridgeChance)
			{
				continue;
			}
			const P48BridgeNetwork::FEdge& Edge = ValidEdges[EdgeIndex];
			const int32 CandidateIndex = P48BridgeConnectionPolicy::FindFirstAvailableCandidate(
				Edge.Candidates,
				Anchors[Edge.A].IslandIndex,
				Anchors[Edge.B].IslandIndex,
				UsedEndpointsByIsland,
				Settings->GenerationSettings.EndpointExclusionRadius);
			if (CandidateIndex != INDEX_NONE)
			{
				P48BridgeNetwork::FSelectedConnection& Selection = SelectedConnections.Emplace_GetRef();
				Selection.EdgeIndex = EdgeIndex;
				Selection.CandidateIndex = CandidateIndex;
				SelectedEdgeSet.Add(EdgeIndex);
				P48BridgeConnectionPolicy::ReserveCandidateEndpoints(
					Edge.Candidates[CandidateIndex],
					Anchors[Edge.A].IslandIndex,
					Anchors[Edge.B].IslandIndex,
					UsedEndpointsByIsland);
			}
		}

		float StaticWeight = 0.0f;
		for (const FP48WeightedBridgeStaticMesh& Entry : Settings->GenerationSettings.StaticMeshes) { StaticWeight += Entry.Mesh ? FMath::Max(0.0f, Entry.Weight) : 0.0f; }
		float ActorWeight = 0.0f;
		for (const FP48WeightedBridgeActor& Entry : Settings->GenerationSettings.ActorClasses) { ActorWeight += Entry.ActorClass ? FMath::Max(0.0f, Entry.Weight) : 0.0f; }

		for (const P48BridgeNetwork::FSelectedConnection& Selection : SelectedConnections)
		{
			const P48BridgeNetwork::FEdge& Edge = ValidEdges[Selection.EdgeIndex];
			const FP48BridgeEndpointCandidate& Candidate = Edge.Candidates[Selection.CandidateIndex];
			// 표면/레거시 입력 모두 선택 단계에서 확정하고 점유 검사한 끝점을 그대로 사용합니다.
			const FVector Start = Candidate.Start;
			const FVector End = Candidate.End;
			const FVector Difference = End - Start;
			const float BridgeLength = Difference.Length();
			if (BridgeLength <= UE_KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const bool bDuplicate = BridgePoints.ContainsByPredicate([&](const P48BridgeNetwork::FBridgePoint& Existing)
			{
				return P48BridgeConnectionPolicy::AreSameUndirectedEndpoints(
					Start,
					End,
					Existing.Start,
					Existing.End,
					Settings->GenerationSettings.DuplicateEndpointTolerance);
			});
			if (bDuplicate)
			{
				UE_LOG(LogTemp, Warning, TEXT("[P48Bridge] Duplicate connection skipped: Start=%s End=%s"), *Start.ToCompactString(), *End.ToCompactString());
				continue;
			}

			P48BridgeNetwork::FBridgePoint& Point = BridgePoints.Emplace_GetRef();
			Point.Start = Start;
			Point.End = End;
			Point.Seed = static_cast<int32>(Random.GetUnsignedInt());
			const bool bSelectStatic = StaticWeight > 0.0f && (ActorWeight <= 0.0f || Random.FRandRange(0.0f, StaticWeight + ActorWeight) <= StaticWeight);
			if (bSelectStatic)
			{
				const FP48WeightedBridgeStaticMesh* Entry = P48BridgeNetwork::SelectWeighted(Settings->GenerationSettings.StaticMeshes, Random, [](const FP48WeightedBridgeStaticMesh& Candidate) { return Candidate.Mesh != nullptr; });
				if (Entry && Entry->Mesh)
				{
					Point.Mesh = Entry->Mesh;
					FVector Scale = Entry->BaseScale;
					Scale.X *= BridgeLength / FMath::Max(1.0f, Entry->NativeLength);
					Point.Transform = FTransform(FRotationMatrix::MakeFromX(Difference).ToQuat(), (Start + End) * 0.5f, Scale);
				}
			}
			else
			{
				const FP48WeightedBridgeActor* Entry = P48BridgeNetwork::SelectWeighted(Settings->GenerationSettings.ActorClasses, Random, [](const FP48WeightedBridgeActor& Candidate) { return Candidate.ActorClass != nullptr; });
				if (Entry && Entry->ActorClass)
				{
					Point.ActorClass = Entry->ActorClass;
					Point.Transform = FTransform(FRotationMatrix::MakeFromX(Difference).ToQuat(), (Start + End) * 0.5f);
				}
			}
		}
	}

	auto AddOutput = [Context](UPCGData* Data, const FName Pin)
	{
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = Data;
		Output.Pin = Pin;
	};
	AddOutput(P48BridgeNetwork::CreateOutputData(Context, BridgePoints, true), UP48PCGBridgeNetworkSettings::StaticMeshOutputLabel);
	AddOutput(P48BridgeNetwork::CreateOutputData(Context, BridgePoints, false), UP48PCGBridgeNetworkSettings::ActorOutputLabel);
	return true;
}

#undef LOCTEXT_NAMESPACE
