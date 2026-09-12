#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Spawn/P48ItemSpawnPoint.h"
#include "../Spawn/P48ItemSpawnSubsystem.h"
#include "Project48/Weapon/P48WeaponBase.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FP48ItemSpawnPointSelectionTest,
	"P48.Items.Spawn.PointSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP48ItemSpawnPointSelectionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AP48ItemSpawnPoint* SpawnPoint = World->SpawnActor<AP48ItemSpawnPoint>();
	if (!TestNotNull(TEXT("Item spawn point"), SpawnPoint))
	{
		World->DestroyWorld(false);
		return false;
	}

	if (!TestTrue(TEXT("Default weapon spawn entries are available"), !SpawnPoint->SpawnEntries.IsEmpty()))
	{
		World->DestroyWorld(false);
		return false;
	}
	SpawnPoint->IslandIndex = 3;
	SpawnPoint->SpawnSlotIndex = 7;

	const TSubclassOf<AP48WeaponBase> FirstSelection = SpawnPoint->SelectWeaponClass(48123);
	const TSubclassOf<AP48WeaponBase> SecondSelection = SpawnPoint->SelectWeaponClass(48123);
	TestEqual(TEXT("The same map and point seed produce the same class"), FirstSelection, SecondSelection);
	TestNotNull(TEXT("A positive-weight class is selected"), FirstSelection.Get());

	SpawnPoint->SpawnHeight = 75.0f;
	TestEqual(
		TEXT("Spawn transform applies the configured height"),
		SpawnPoint->GetItemSpawnTransform().GetLocation().Z,
		75.0);

	UP48ItemSpawnSubsystem* Subsystem = World->GetSubsystem<UP48ItemSpawnSubsystem>();
	if (TestNotNull(TEXT("Item spawn subsystem"), Subsystem))
	{
		AP48WeaponBase* FirstItem = Subsystem->SpawnAtPoint(SpawnPoint);
		TestNotNull(TEXT("A valid point spawns an item on the server"), FirstItem);
		TestEqual(
			TEXT("Repeated requests do not duplicate the point's item"),
			Subsystem->SpawnAtPoint(SpawnPoint),
			FirstItem);
	}

	World->DestroyWorld(false);
	return true;
}

#endif
