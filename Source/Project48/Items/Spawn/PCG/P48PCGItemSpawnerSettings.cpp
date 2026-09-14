#include "P48PCGItemSpawnerSettings.h"

#include "Project48/Maps/PCG/Common/P48PCGSpawnAttributeNames.h"
#include "Data/PCGPointData.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGComponent.h"
#include "PCGManagedResource.h"

#define LOCTEXT_NAMESPACE "P48ItemSpawner"

UP48PCGItemSpawnerSettings::UP48PCGItemSpawnerSettings()
	: IslandIndexAttribute(P48PCGSpawnAttributeNames::IslandIndex)
	, SpawnSlotIndexAttribute(P48PCGSpawnAttributeNames::SpawnSlotIndex)
	, GenerationIdAttribute(P48PCGSpawnAttributeNames::GenerationId)
{
	const AP48ItemSpawnPoint* SpawnPointDefaults = GetDefault<AP48ItemSpawnPoint>();
	if (SpawnPointDefaults)
	{
		SpawnEntries = SpawnPointDefaults->SpawnEntries;
		SpawnHeight = SpawnPointDefaults->SpawnHeight;
		bAutoSpawn = SpawnPointDefaults->bAutoSpawn;
		SeedOffset = SpawnPointDefaults->SeedOffset;
	}
}

#if WITH_EDITOR
FText UP48PCGItemSpawnerSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("Title", "P48 Item Spawner");
}

FText UP48PCGItemSpawnerSettings::GetNodeTooltipText() const
{
	return LOCTEXT(
		"Tooltip",
		"Spawns one P48 item spawn-point actor per input point and automatically reads the island, slot, and generation attributes.");
}
#endif

TArray<FPCGPinProperties> UP48PCGItemSpawnerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	return Pins;
}

TArray<FPCGPinProperties> UP48PCGItemSpawnerSettings::OutputPinProperties() const
{
	return Super::DefaultPointOutputPinProperties();
}

FPCGElementPtr UP48PCGItemSpawnerSettings::CreateElement() const
{
	return MakeShared<FP48PCGItemSpawnerElement>();
}

bool FP48PCGItemSpawnerElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	const auto* Settings = Context->GetInputSettings<UP48PCGItemSpawnerSettings>();
	if (!Settings)
	{
		return true;
	}

	UPCGComponent* SourceComponent = Cast<UPCGComponent>(Context->ExecutionSource.Get());
	UWorld* World = Context->ExecutionSource.IsValid()
		? Context->ExecutionSource->GetExecutionState().GetWorld()
		: nullptr;
	if (!SourceComponent || !World)
	{
		PCGE_LOG(
			Error,
			GraphAndLog,
			LOCTEXT("MissingExecutionSource", "P48 Item Spawner requires a PCG Component execution source."));
		return true;
	}

	UPCGManagedActors* ManagedActors = nullptr;
	int32 InputPointCount = 0;
	int32 SpawnedActorCount = 0;

	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
	{
		const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(Input.Data);
		if (!PointData || !PointData->Metadata)
		{
			continue;
		}

		const FPCGMetadataAttribute<int32>* IslandIndices =
			PointData->Metadata->GetConstTypedAttribute<int32>(Settings->IslandIndexAttribute);
		const FPCGMetadataAttribute<int32>* SpawnSlotIndices =
			PointData->Metadata->GetConstTypedAttribute<int32>(Settings->SpawnSlotIndexAttribute);
		const FPCGMetadataAttribute<int32>* GenerationIds =
			PointData->Metadata->GetConstTypedAttribute<int32>(Settings->GenerationIdAttribute);
		if (!IslandIndices || !SpawnSlotIndices || !GenerationIds)
		{
			PCGE_LOG(
				Error,
				GraphAndLog,
				LOCTEXT(
					"MissingRequiredAttributes",
					"P48 Item Spawner requires IslandIndex, SpawnSlotIndex, and GenerationId attributes. Connect the P48 Item Spawn Point output directly."));
			continue;
		}

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
		InputPointCount += PointData->GetNumPoints();

		// 실제 무기는 서버에서 생성되어 복제되므로 게임 클라이언트에는 스폰 포인트를 만들지 않습니다.
		if (World->IsGameWorld() && World->GetNetMode() == NM_Client)
		{
			continue;
		}

		AActor* TargetActor = Context->GetTargetActor(PointData);
		for (int32 PointIndex = 0; PointIndex < PointData->GetNumPoints(); ++PointIndex)
		{
			const PCGMetadataEntryKey Entry = PointData->GetMetadataEntry(PointIndex);
			const FTransform SpawnTransform = PointData->GetTransform(PointIndex);
			AP48ItemSpawnPoint* SpawnPoint = World->SpawnActorDeferred<AP48ItemSpawnPoint>(
				AP48ItemSpawnPoint::StaticClass(),
				SpawnTransform,
				TargetActor,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!SpawnPoint)
			{
				PCGE_LOG(
					Error,
					GraphAndLog,
					LOCTEXT("SpawnFailed", "P48 Item Spawner failed to create an item spawn-point actor."));
				continue;
			}

			SpawnPoint->IslandIndex = IslandIndices->GetValueFromItemKey(Entry);
			SpawnPoint->SpawnSlotIndex = SpawnSlotIndices->GetValueFromItemKey(Entry);
			SpawnPoint->GenerationId = GenerationIds->GetValueFromItemKey(Entry);
			SpawnPoint->SpawnEntries = Settings->SpawnEntries;
			SpawnPoint->SpawnHeight = Settings->SpawnHeight;
			SpawnPoint->bAutoSpawn = Settings->bAutoSpawn;
			SpawnPoint->SeedOffset = Settings->SeedOffset;
			UGameplayStatics::FinishSpawningActor(SpawnPoint, SpawnTransform);

			if (!ManagedActors)
			{
				ManagedActors = NewObject<UPCGManagedActors>(SourceComponent);
#if WITH_EDITOR
				ManagedActors->SetIsPreview(SourceComponent->IsInPreviewMode());
#endif
			}
			ManagedActors->GetMutableGeneratedActors().Add(SpawnPoint);
			++SpawnedActorCount;
		}
	}

	if (ManagedActors)
	{
		SourceComponent->AddToManagedResources(ManagedActors);
	}

	if (Settings->bLogDiagnostics)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[P48ItemSpawner] InputPoints=%d SpawnedActors=%d"),
			InputPointCount,
			SpawnedActorCount);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
