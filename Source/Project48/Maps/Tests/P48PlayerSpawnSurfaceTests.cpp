#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../PCG/Surface/P48IslandSurfaceSampler.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP48PawnSupportTest, "P48.Maps.Spawn.PawnSupport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP48PawnSupportTest::RunTest(const FString& Parameters)
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Collision test mesh"), Mesh)) { return false; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* Owner = World->SpawnActor<AActor>();
	UStaticMeshComponent* Surface = NewObject<UStaticMeshComponent>(Owner);
	Owner->SetRootComponent(Surface);
	Surface->SetStaticMesh(Mesh);
	Surface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Surface->SetCollisionObjectType(ECC_WorldStatic);
	Surface->SetCollisionResponseToAllChannels(ECR_Block);
	Surface->RegisterComponent();
	const FTransform Transform = Surface->GetComponentTransform();
	const FBox Bounds = Mesh->GetBounds().TransformBy(Transform).GetBox();
	FHitResult Hit;
	TestTrue(TEXT("Pawn-blocking simple collision is accepted"), FP48IslandSurfaceSampler::TraceTop(World, Mesh, Transform, Bounds, FVector::ZeroVector, Hit, nullptr, true));
	Surface->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	TestFalse(TEXT("A visible surface that ignores Pawn is not a spawn surface"), FP48IslandSurfaceSampler::TraceTop(World, Mesh, Transform, Bounds, FVector::ZeroVector, Hit, nullptr, true));
	Surface->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	FTransform WrongIsland = Transform;
	WrongIsland.AddToTranslation(FVector(1000, 0, 0));
	TestFalse(TEXT("The same mesh on another island cannot satisfy support"), FP48IslandSurfaceSampler::TraceTop(World, Mesh, WrongIsland, Bounds, FVector::ZeroVector, Hit, nullptr, true));
	World->DestroyWorld(false);
	return true;
}

#endif
