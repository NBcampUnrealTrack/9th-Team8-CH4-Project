#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Objects/Bridge/P48SwingBridge.h"
#include "../Components/Layout/P48BridgeLayoutComponent.h"
#include "../Components/Measurement/P48MeshBoundsComponent.h"
#include "../Components/Rope/P48BridgeRopePathComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP48IntegratedPostTest, "P48.Maps.IntegratedPost.SocketsAndClearance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP48IntegratedPostTest::RunTest(const FString& Parameters)
{
	UStaticMesh* PostMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/MSJ/Bridge/SM_Bridge_AnchorPost/StaticMeshes/SM_Bridge_AnchorPost.SM_Bridge_AnchorPost"));
	UStaticMesh* PlankMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/MSJ/Bridge/SM_Bridge_Plank/StaticMeshes/SM_Bridge_Plank.SM_Bridge_Plank"));
	if (!TestNotNull(TEXT("Project integrated post"), PostMesh) || !TestNotNull(TEXT("Project plank"), PlankMesh)) { return false; }
	if (!TestNotNull(TEXT("Upper socket saved on asset"), PostMesh->FindSocket(TEXT("UpperRopeAnchor"))) || !TestNotNull(TEXT("Lower socket saved on asset"), PostMesh->FindSocket(TEXT("LowerRopeAnchor")))) { return false; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AP48SwingBridge* Actor = World->SpawnActor<AP48SwingBridge>();
	Actor->SetActorTransform(FTransform(FRotator(0.0f, 35.0f, 0.0f), FVector(100.0f, 200.0f, 300.0f)));
	auto* Layout = Actor->FindComponentByClass<UP48BridgeLayoutComponent>();
	auto* Measurement = Actor->FindComponentByClass<UP48MeshBoundsComponent>();
	auto* Rope = Actor->FindComponentByClass<UP48BridgeRopePathComponent>();
	FP48SwingBridgeSettings Settings;
	Settings.Assets.Plank.Mesh = PlankMesh;
	Settings.Assets.AnchorPost.Mesh = PostMesh;
	Settings.Assets.Plank.Scale = FVector(2.0f, 3.0f, 1.0f);
	Settings.Assets.AnchorPost.Scale = FVector(1.3f, 0.8f, 1.0f);
	FP48MeasuredMeshBounds Bounds;
	Measurement->MeasureMesh(PlankMesh, Settings.Assets.Plank.Scale, Bounds);
	FP48BridgeLayoutResult Result;
	if (!TestTrue(TEXT("Sloped layout"), Layout->CalculateLayout(FVector::ZeroVector, FVector(5000.0f, 2000.0f, 600.0f), Settings, Bounds, Result)))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("Four posts"), Result.AnchorPostTransforms.Num(), 4);
	TestTrue(TEXT("Integrated post skips separate knots"), Result.UpperPostKnotTransforms.IsEmpty() && Result.LowerPostKnotTransforms.IsEmpty());
	const FVector Right = FVector::CrossProduct(FVector::UpVector, FVector(5000.0f, 2000.0f, 600.0f)).GetSafeNormal();
	auto ExtentOnAxis = [](UStaticMesh* Mesh, const FTransform& Transform, const FVector& Axis)
	{
		const FVector E = Mesh->GetBounds().BoxExtent * Transform.GetScale3D().GetAbs();
		return FMath::Abs(FVector::DotProduct(Axis, Transform.GetUnitAxis(EAxis::X))) * E.X + FMath::Abs(FVector::DotProduct(Axis, Transform.GetUnitAxis(EAxis::Y))) * E.Y + FMath::Abs(FVector::DotProduct(Axis, Transform.GetUnitAxis(EAxis::Z))) * E.Z;
	};
	TArray<TObjectPtr<UStaticMeshComponent>> Posts;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FTransform& Post = Result.AnchorPostTransforms[Index];
		const FTransform& Plank = Index < 2 ? Result.PlankTransforms[0] : Result.PlankTransforms.Last();
		const FVector Side = Index % 2 == 0 ? -Right : Right;
		const double Gap = FVector::DotProduct(Side, Post.TransformPosition(PostMesh->GetBounds().Origin)) - ExtentOnAxis(PostMesh, Post, Side) - FVector::DotProduct(Side, Plank.TransformPosition(PlankMesh->GetBounds().Origin)) - ExtentOnAxis(PlankMesh, Plank, Side);
		TestTrue(TEXT("Post is outside plank including mesh scale and pivot"), Gap >= Settings.PostConnection.PostClearance - 0.01);
		const FVector Inward = (Index < 2 ? 1.0 : -1.0) * FVector(5000.0f, 2000.0f, 0.0f).GetSafeNormal();
		const double EntranceGap = FVector::DotProduct(Inward, Plank.TransformPosition(PlankMesh->GetBounds().Origin)) - ExtentOnAxis(PlankMesh, Plank, Inward) - FVector::DotProduct(Inward, Post.TransformPosition(PostMesh->GetBounds().Origin)) - ExtentOnAxis(PostMesh, Post, Inward);
		TestTrue(TEXT("Deck begins beyond the inner face of each post"), EntranceGap >= Settings.PostConnection.PostClearance - 0.01);
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Actor);
		Component->SetStaticMesh(PostMesh);
		Component->SetWorldTransform(Post);
		Posts.Add(Component);
		TestTrue(TEXT("Lower socket defines endpoint height"), FMath::IsNearlyEqual(Component->GetSocketLocation(TEXT("LowerRopeAnchor")).Z, Index < 2 ? 0.0 : 600.0, 0.001));
	}
	FString Error;
	TestTrue(TEXT("Read saved sockets"), Rope->ConfigurePostSockets(Posts, Settings.PostConnection, Error));
	Result.Nodes[Result.Nodes.Num() / 2].CurrentLeft.Z -= 40.0f;
	FP48BridgeRopePathResult Paths;
	TestTrue(TEXT("Rope paths"), Rope->CalculatePaths(Result.Nodes, Settings.Layout.MainRopeHeight, 4.0f, Paths));
	TestEqual(TEXT("Independent socket points surround plank points"), Paths.LeftMainPoints.Num(), Result.Nodes.Num() + 2);
	TestEqual(TEXT("Only two vertical connectors per plank, no X braces"), Paths.ConnectorSegments.Num(), Result.Nodes.Num() * 2);
	for (const FP48BridgeRopeSegment& Connector : Paths.ConnectorSegments)
	{
		const FVector Delta = Actor->GetActorTransform().TransformVector(Connector.End - Connector.Start);
		TestTrue(TEXT("Connector stays vertical"), FVector2D(Delta.X, Delta.Y).IsNearlyZero(0.001));
	}
	const TArray<FP48BridgeRopeSegment>* Segments[] = { &Paths.LeftMainSegments, &Paths.RightMainSegments, &Paths.LeftLowerSegments, &Paths.RightLowerSegments };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (!TestFalse(TEXT("Rope exists"), Segments[Index]->IsEmpty())) { continue; }
		const FName Name = Index < 2 ? TEXT("UpperRopeAnchor") : TEXT("LowerRopeAnchor");
		const int32 Side = Index % 2;
		TestTrue(TEXT("Socket rotation cannot bend endpoint tangent"), (*Segments[Index])[0].StartTangent.Equals((*Segments[Index])[0].End - (*Segments[Index])[0].Start, 0.001));
		TestTrue(TEXT("Start stays on socket after plank movement"), Actor->GetActorTransform().TransformPosition((*Segments[Index])[0].Start).Equals(Posts[Side]->GetSocketLocation(Name), 0.001));
		TestTrue(TEXT("End stays on socket after plank movement"), Actor->GetActorTransform().TransformPosition(Segments[Index]->Last().End).Equals(Posts[Side + 2]->GetSocketLocation(Name), 0.001));
	}
	Settings.PostConnection.bGenerateLowerRopes = false;
	FP48BridgeLayoutResult ShortResult;
	TestFalse(TEXT("Too-short span rejected instead of overlapping planks"), Layout->CalculateLayout(FVector::ZeroVector, FVector(10.0f, 0.0f, 0.0f), Settings, Bounds, ShortResult));
	TestTrue(TEXT("Lower ropes optional"), Rope->ConfigurePostSockets(Posts, Settings.PostConnection, Error));
	Rope->CalculatePaths(Result.Nodes, 120.0f, 4.0f, Paths);
	TestTrue(TEXT("No lower segments when disabled"), Paths.LeftLowerSegments.IsEmpty() && Paths.RightLowerSegments.IsEmpty());
	Settings.PostConnection.UpperSocketName = TEXT("InvalidSocket");
	TestFalse(TEXT("Invalid socket rejected without silently using actor origin"), Rope->ConfigurePostSockets(Posts, Settings.PostConnection, Error));
	World->DestroyWorld(false);
	return true;
}

#endif
