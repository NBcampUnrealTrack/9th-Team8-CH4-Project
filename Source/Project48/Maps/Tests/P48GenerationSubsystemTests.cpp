#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../PCG/Common/P48PCGNetworkSeedSettings.h"
#include "../PCG/Common/P48PCGSeedWorldSubsystem.h"
#include "../PCG/Common/P48PCGSeedState.h"
#include "../Objects/Spawn/P48PlayerStart.h"
#include "../Objects/Spawn/P48PlayerStartRegistrySubsystem.h"
#include "Engine/World.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "../PCG/Common/P48PCGSeedHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FP48GenerationContextTest,
	"P48.Maps.Generation.SubsystemContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP48GenerationContextTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("NetworkSeed produces a non-zero server seed"), UP48PCGNetworkSeedSettings::GenerateServerSeed() > 0);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UP48PCGSeedWorldSubsystem* Coordinator = NewObject<UP48PCGSeedWorldSubsystem>(World);
	TestNotNull(TEXT("Generation coordinator"), Coordinator);

	FP48PCGGenerationSnapshot Snapshot;
	Snapshot.Seed = 48123;
	Snapshot.Revision = 7;
	Snapshot.RequiredPlayerCount = 0;
	Snapshot.Phase = EP48PCGGenerationPhase::Cleaning;
	TestTrue(TEXT("Seed and revision are enough to start map generation"), Snapshot.HasValidSeed());
	TestFalse(TEXT("Player count can remain unconfirmed during map generation"), Snapshot.HasConfirmedPlayerCount());
	Coordinator->HandleReplicatedSnapshot(Snapshot);

	const FP48PCGGenerationContext Context = Coordinator->GetGenerationContext();
	TestEqual(TEXT("Seed is stable for the generation"), Context.Seed, 48123);
	TestEqual(TEXT("Revision becomes GenerationId"), Context.GenerationId, 7);
	TestEqual(TEXT("Map generation does not require a confirmed player count"), Context.RequiredPlayerCount, 0);
	TestTrue(TEXT("Seed and revision form a valid map generation context"), Context.IsValid());

	Coordinator->SetRequiredPlayerCount(2);
	TestEqual(TEXT("First confirmed player count locks the PlayerStart layout"), Coordinator->GetGenerationContext().RequiredPlayerCount, 2);
	Coordinator->SetRequiredPlayerCount(3);
	TestEqual(TEXT("Late joins do not expand the locked PlayerStart layout"), Coordinator->GetGenerationContext().RequiredPlayerCount, 2);
	Coordinator->SetRequiredPlayerCount(1);
	TestEqual(TEXT("Participant departures may reduce the required PlayerStart count"), Coordinator->GetGenerationContext().RequiredPlayerCount, 1);
	Coordinator->SetRequiredPlayerCount(0);
	Coordinator->SetRequiredPlayerCount(2);
	TestEqual(TEXT("An empty locked roster cannot be reopened by a late join"), Coordinator->GetGenerationContext().RequiredPlayerCount, 0);

	AActor* PCGOwner = World->SpawnActor<AActor>();
	UPCGComponent* PCGComponent = NewObject<UPCGComponent>(PCGOwner);
	PCGComponent->Seed = 42;
	Coordinator->RegisterConsumer(PCGComponent);

	Snapshot.Seed = 91234;
	Snapshot.Revision = 8;
	Coordinator->HandleReplicatedSnapshot(Snapshot);
	TestEqual(TEXT("Replicated seed is applied to every PCG consumer before generation"), PCGComponent->Seed, 91234);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FP48PlayerStartRegistryTest,
	"P48.Maps.Generation.PlayerStartRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP48PlayerStartRegistryTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UP48PlayerStartRegistrySubsystem* Registry = NewObject<UP48PlayerStartRegistrySubsystem>(World);
	TestNotNull(TEXT("PlayerStart registry"), Registry);

	Registry->BeginGeneration(11, 0);
	AP48PlayerStart* First = World->SpawnActor<AP48PlayerStart>();
	AP48PlayerStart* Second = World->SpawnActor<AP48PlayerStart>();
	First->SpawnSlotIndex = 0;
	Second->SetActorLocation(FVector(1000, 0, 0));
	TestFalse(TEXT("PlayerStarts cannot register before the current layout is selected"), Registry->RegisterPlayerStart(First));
	Registry->ReportSelectedStarts(11, {{First->GetActorLocation(), 3}, {Second->GetActorLocation(), 7}});
	TestTrue(TEXT("First current-layout start registers"), Registry->RegisterPlayerStart(First));
	Second->SpawnSlotIndex = 0;
	TestFalse(TEXT("A wrong explicit slot is rejected instead of silently normalized"), Registry->RegisterPlayerStart(Second));
	Second->SpawnSlotIndex = INDEX_NONE;
	TestTrue(TEXT("Second current-layout start registers"), Registry->RegisterPlayerStart(Second));

	TestEqual(TEXT("Unmapped slot is resolved from the selected position"), Second->SpawnSlotIndex, 1);
	TestEqual(TEXT("Unmapped island is resolved from the selected position"), Second->IslandIndex, 7);
	TestEqual(TEXT("Unique starts are counted"), Registry->GetRegisteredCount(11), 2);
	TestEqual(TEXT("PlayerStart candidates remain pending before player count confirmation"), Registry->GetState(11), EP48PlayerStartLayoutState::Pending);
	TArray<AP48PlayerStart*> ReadyStarts;
	TestFalse(TEXT("Pending layouts do not expose spawn points"), Registry->GetReadyPlayerStarts(11, ReadyStarts));
	TestEqual(TEXT("Failed queries clear the output array"), ReadyStarts.Num(), 0);
	Registry->UpdateRequiredCount(11, 2);
	TestEqual(TEXT("Matching selection and registration is ready"), Registry->GetState(11), EP48PlayerStartLayoutState::Ready);
	TestTrue(TEXT("The active ready generation exposes spawn points"), Registry->GetReadyPlayerStarts(11, ReadyStarts));
	TestEqual(TEXT("All required spawn points are returned"), ReadyStarts.Num(), 2);
	if (ReadyStarts.Num() == 2)
	{
		TestEqual(TEXT("Spawn points are sorted by slot"), ReadyStarts[0]->SpawnSlotIndex, 0);
		TestEqual(TEXT("The second spawn slot follows the first"), ReadyStarts[1]->SpawnSlotIndex, 1);
		TestEqual(TEXT("Returned starts belong to the active generation"), ReadyStarts[0]->GenerationId, 11);
		TestEqual(TEXT("Every returned start belongs to the active generation"), ReadyStarts[1]->GenerationId, 11);
	}
	TestEqual(TEXT("Old revisions cannot read the active registry"), Registry->GetRegisteredCount(10), 0);
	TestFalse(TEXT("Old revisions cannot retrieve ready spawn points"), Registry->GetReadyPlayerStarts(10, ReadyStarts));
	TestEqual(TEXT("A stale query does not retain active spawn points"), ReadyStarts.Num(), 0);

	AP48PlayerStart* Stale = World->SpawnActor<AP48PlayerStart>();
	Stale->GenerationId = 10;
	TestFalse(TEXT("A stale generation cannot enter the active registry"), Registry->RegisterPlayerStart(Stale));
	Registry->UnregisterPlayerStart(Second);
	TestEqual(TEXT("Losing a required start invalidates readiness"), Registry->GetState(11), EP48PlayerStartLayoutState::Pending);
	TestFalse(TEXT("An incomplete layout no longer exposes spawn points"), Registry->GetReadyPlayerStarts(11, ReadyStarts));
	TestEqual(TEXT("An incomplete query clears the output array"), ReadyStarts.Num(), 0);
	AP48PlayerStart* WrongPosition = World->SpawnActor<AP48PlayerStart>();
	WrongPosition->SetActorLocation(FVector(500, 0, 0));
	TestFalse(TEXT("Count alone cannot admit an unselected position"), Registry->RegisterPlayerStart(WrongPosition));
	Registry->BeginGeneration(12, 2);
	TestEqual(TEXT("New generations clear registrations"), Registry->GetRegisteredCount(12), 0);
	TestTrue(TEXT("Registry does not destroy actors owned by PCG or the level"), IsValid(First));

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FP48EarlyPlayerCountTest,
	"P48.Maps.Generation.PlayerCountBeforeSeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP48EarlyPlayerCountTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UP48PCGSeedWorldSubsystem* Coordinator = World->GetSubsystem<UP48PCGSeedWorldSubsystem>();
	if (!TestNotNull(TEXT("World-owned coordinator"), Coordinator))
	{
		World->DestroyWorld(false);
		return false;
	}
	Coordinator->SetRequiredPlayerCount(2);
	AP48PCGSeedState* State = World->SpawnActor<AP48PCGSeedState>();
	Coordinator->SetReplicatedState(State);
	Coordinator->RequestGenerationWithSeed(12345);
	TestEqual(TEXT("First Seed creation preserves an already confirmed roster"), Coordinator->GetSnapshot().RequiredPlayerCount, 2);
	TestEqual(TEXT("Replicated snapshot contains the preserved count"), State->State.RequiredPlayerCount, 2);
	TestEqual(TEXT("Seed remains Maps-owned"), State->State.Seed, 12345);
	AActor* Owner = World->SpawnActor<AActor>();
	UPCGComponent* Component = NewObject<UPCGComponent>(Owner);
	FPCGContext PCGContext;
	PCGContext.ExecutionSource = TWeakInterfacePtr<IPCGGraphExecutionSource>(Component);
	UPCGParamData* StaleParams = NewObject<UPCGParamData>();
	const PCGMetadataEntryKey Entry = StaleParams->Metadata->AddEntry();
	StaleParams->Metadata->CreateAttribute<int32>(P48PCGSeedNames::Seed, 42, false, false)->SetValue(Entry, 42);
	StaleParams->Metadata->CreateAttribute<int32>(P48PCGSeedNames::GenerationId, 99, false, false)->SetValue(Entry, 99);
	StaleParams->Metadata->CreateAttribute<int32>(P48PCGSeedNames::RequiredPlayerCount, 8, false, false)->SetValue(Entry, 8);
	FPCGTaggedData& Tagged = PCGContext.InputData.TaggedData.Emplace_GetRef();
	Tagged.Pin = P48PCGSeedNames::InputPin;
	Tagged.Data = StaleParams;
	FP48PCGGenerationContext ReadContext;
	TestTrue(TEXT("Runtime context can be read"), P48ReadGenerationContext(&PCGContext, ReadContext));
	TestEqual(TEXT("Stale ParamData cannot replace runtime seed"), ReadContext.Seed, 12345);
	TestEqual(TEXT("Seed and revision are read from the same snapshot"), ReadContext.GenerationId, State->State.Revision);
	TestEqual(TEXT("Stale ParamData cannot replace the roster"), ReadContext.RequiredPlayerCount, 2);
	Coordinator->HandleReplicatedSnapshot(FP48PCGGenerationSnapshot());
	TestEqual(TEXT("Default snapshots cannot erase an active generation"), Coordinator->GetSnapshot().Seed, 12345);
	Coordinator->ApplyDebugPlayerCount(4);
	TestEqual(TEXT("Debug count replaces the roster without regenerating the map"), Coordinator->GetSnapshot().RequiredPlayerCount, 4);
	TestEqual(TEXT("Debug override preserves seed"), Coordinator->GetSnapshot().Seed, 12345);
	TestEqual(TEXT("Debug override preserves generation"), Coordinator->GetSnapshot().Revision, ReadContext.GenerationId);
	Coordinator->SetRequiredPlayerCount(1);
	TestEqual(TEXT("Broadcast count cannot replace the active debug override"), Coordinator->GetSnapshot().RequiredPlayerCount, 4);

	World->DestroyWorld(false);
	return true;
}

#endif
