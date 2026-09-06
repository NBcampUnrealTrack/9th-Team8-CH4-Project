#include "P48PCGBridgeNetworkSettings.h"
#include "../Common/P48PCGSeedHelpers.h"

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
	struct FAnchor
	{
		FVector Location = FVector::ZeroVector;
		float Radius = 0.0f;
		int32 IslandIndex = INDEX_NONE;
		TArray<FVector> SurfaceCandidates;
	};

	struct FEdge
	{
		int32 A = INDEX_NONE;
		int32 B = INDEX_NONE;
		float Length = 0.0f;
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
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

	bool IsValidEdge(const FAnchor& A, const FAnchor& B, const FP48BridgeConnectionRules& Rules, float& OutLength)
	{
		const FVector Difference = B.Location - A.Location;
		const float HorizontalDistance = FVector2D(Difference.X, Difference.Y).Length();
		const float HeightDifference = FMath::Abs(Difference.Z);
		OutLength = Difference.Length();
		if (HorizontalDistance < Rules.MinHorizontalDistance || HeightDifference > Rules.MaxHeightDifference || OutLength > Rules.MaxBridgeLength)
		{
			return false;
		}

		return FMath::RadiansToDegrees(FMath::Atan2(HeightDifference, HorizontalDistance)) <= Rules.MaxSlopeAngle;
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
	return LOCTEXT("NodeTooltip", "Accepts Bridge Surface Points (IslandIndex + SurfaceNormal) or legacy Connection Anchors. Surface candidates are grouped by island; the shortest valid pair per island pair enters Kruskal MST. Surface positions are used unchanged; AnchorInset applies only to legacy anchors. Post footprint and obstacle clearance are not tested.");
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
		if (bSurfaceInput && !IslandIndexAttribute)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingIslandId", "Surface points require IslandIndex to group points by island."));
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
				Anchors[*Group].SurfaceCandidates.Add(InputPoints->GetTransform(Index).GetLocation());
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
				if (bSurfaceInput)
				{
					// 섬 쌍마다 경사/높이/길이를 통과하는 가장 짧은 후보 쌍 하나만 MST에 전달합니다.
					P48BridgeNetwork::FEdge Best;
					Best.A = A;
					Best.B = B;
					Best.Length = TNumericLimits<float>::Max();
					for (const FVector& StartCandidate : Anchors[A].SurfaceCandidates)
					{
						for (const FVector& EndCandidate : Anchors[B].SurfaceCandidates)
						{
							P48BridgeNetwork::FAnchor StartAnchor;
							P48BridgeNetwork::FAnchor EndAnchor;
							StartAnchor.Location = StartCandidate;
							EndAnchor.Location = EndCandidate;
							float CandidateLength = 0.0f;
							if (P48BridgeNetwork::IsValidEdge(StartAnchor, EndAnchor, Settings->GenerationSettings.ConnectionRules, CandidateLength) && CandidateLength < Best.Length)
							{
								Best.Length = CandidateLength;
								Best.Start = StartCandidate;
								Best.End = EndCandidate;
							}
						}
					}
					if (Best.Length < TNumericLimits<float>::Max()) { ValidEdges.Add(Best); }
					continue;
				}
				float Length = 0.0f;
				if (P48BridgeNetwork::IsValidEdge(Anchors[A], Anchors[B], Settings->GenerationSettings.ConnectionRules, Length))
				{
					P48BridgeNetwork::FEdge& Edge = ValidEdges.Emplace_GetRef();
					Edge.A = A;
					Edge.B = B;
					Edge.Length = Length;
				}
			}
		}

		ValidEdges.Sort([](const P48BridgeNetwork::FEdge& Left, const P48BridgeNetwork::FEdge& Right) { return Left.Length < Right.Length; });
		P48BridgeNetwork::FDisjointSet Sets(Anchors.Num());
		TArray<int32> SelectedEdgeIndices;
		TSet<int32> SelectedEdgeSet;
		for (int32 EdgeIndex = 0; EdgeIndex < ValidEdges.Num(); ++EdgeIndex)
		{
			const P48BridgeNetwork::FEdge& Edge = ValidEdges[EdgeIndex];
			if (Sets.Union(Edge.A, Edge.B))
			{
				SelectedEdgeIndices.Add(EdgeIndex);
				SelectedEdgeSet.Add(EdgeIndex);
				if (SelectedEdgeIndices.Num() == Anchors.Num() - 1)
				{
					break;
				}
			}
		}

		if (SelectedEdgeIndices.Num() != Anchors.Num() - 1)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("DisconnectedGraph", "The bridge rules cannot connect every island. No complete MST exists."));
		}

		FRandomStream Random(PCGHelpers::ComputeSeed(Settings->GenerationSettings.RandomSeed, P48ReadNetworkSeed(Context)));
		for (int32 EdgeIndex = 0; EdgeIndex < ValidEdges.Num(); ++EdgeIndex)
		{
			if (!SelectedEdgeSet.Contains(EdgeIndex) && Random.FRand() <= Settings->GenerationSettings.AdditionalBridgeChance)
			{
				SelectedEdgeIndices.Add(EdgeIndex);
			}
		}

		float StaticWeight = 0.0f;
		for (const FP48WeightedBridgeStaticMesh& Entry : Settings->GenerationSettings.StaticMeshes) { StaticWeight += Entry.Mesh ? FMath::Max(0.0f, Entry.Weight) : 0.0f; }
		float ActorWeight = 0.0f;
		for (const FP48WeightedBridgeActor& Entry : Settings->GenerationSettings.ActorClasses) { ActorWeight += Entry.ActorClass ? FMath::Max(0.0f, Entry.Weight) : 0.0f; }

		for (const int32 EdgeIndex : SelectedEdgeIndices)
		{
			const P48BridgeNetwork::FEdge& Edge = ValidEdges[EdgeIndex];
			FVector Start;
			FVector End;
			if (bSurfaceInput)
			{
				// 이미 표면에서 검증된 위치이므로 Bounds/AnchorInset으로 다시 이동시키지 않습니다.
				Start = Edge.Start;
				End = Edge.End;
			}
			else
			{
				P48BridgeNetwork::CalculateEndpoints(Anchors[Edge.A], Anchors[Edge.B], Settings->GenerationSettings.AnchorInset, Start, End);
			}
			const FVector Difference = End - Start;
			const float BridgeLength = Difference.Length();
			if (BridgeLength <= UE_KINDA_SMALL_NUMBER)
			{
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
