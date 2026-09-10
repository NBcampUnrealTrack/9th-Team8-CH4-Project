#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../PCG/Common/P48PCGNetworkSeedSettings.h"
#include "../PCG/Common/P48PCGSeedWorldSubsystem.h"
#include "../Objects/Spawn/P48PlayerStart.h"
#include "../Objects/Spawn/P48PlayerStartRegistrySubsystem.h"
#include "Engine/World.h"
#include "PCGComponent.h"

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
	Second->SpawnSlotIndex = 0;
	Registry->RegisterPlayerStart(First);
	Registry->RegisterPlayerStart(Second);
	Registry->ReportSelectedLayout(11, 2);

	TestEqual(TEXT("Duplicate slots are normalized"), Second->SpawnSlotIndex, 1);
	TestEqual(TEXT("Unique starts are counted"), Registry->GetRegisteredCount(11), 2);
	TestEqual(TEXT("PlayerStart candidates remain pending before player count confirmation"), Registry->GetState(11), EP48PlayerStartLayoutState::Pending);
	Registry->UpdateRequiredCount(11, 2);
	TestEqual(TEXT("Matching selection and registration is ready"), Registry->GetState(11), EP48PlayerStartLayoutState::Ready);
	TestEqual(TEXT("Old revisions cannot read the active registry"), Registry->GetRegisteredCount(10), 0);

	World->DestroyWorld(false);
	return true;
}

#endif
