#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP48MetadataEntryTest, "P48.Maps.Metadata.RepeatedValues", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP48MetadataEntryTest::RunTest(const FString& Parameters)
{
	UPCGMetadata* Metadata = NewObject<UPCGMetadata>();
	auto* Mesh = Metadata->CreateAttribute<FSoftObjectPath>(TEXT("Mesh"), FSoftObjectPath(), false, false);
	auto* Island = Metadata->CreateAttribute<int32>(TEXT("IslandIndex"), INDEX_NONE, false, false);
	auto* Radius = Metadata->CreateAttribute<float>(TEXT("PlacementRadius"), 0.0f, false, false);
	auto* CanSpawn = Metadata->CreateAttribute<bool>(TEXT("CanSpawnPlayer"), false, false, false);
	TArray<PCGMetadataEntryKey> Entries;
	for (int32 Index = 0; Index < 14; ++Index)
	{
		const PCGMetadataEntryKey Entry = Metadata->AddEntry();
		Entries.Add(Entry);
		Mesh->SetValue(Entry, FSoftObjectPath(FString::Printf(TEXT("/Game/Test/Island%d.Island%d"), Index % 6, Index % 6)));
		Island->SetValue(Entry, Index / 2);
		Radius->SetValue(Entry, 100.0f);
		CanSpawn->SetValue(Entry, true);
	}
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const PCGMetadataEntryKey Entry = Entries[Index];
		TestEqual(TEXT("Repeated mesh is resolved by point entry"), Mesh->GetValueFromItemKey(Entry).ToString(), FString::Printf(TEXT("/Game/Test/Island%d.Island%d"), Index % 6, Index % 6));
		TestEqual(TEXT("Repeated island IDs remain grouped"), Island->GetValueFromItemKey(Entry), Index / 2);
		TestEqual(TEXT("Shared radius remains available"), Radius->GetValueFromItemKey(Entry), 100.0f);
		TestTrue(TEXT("Shared spawn flag remains available"), CanSpawn->GetValueFromItemKey(Entry));
	}
	return true;
}

#endif
