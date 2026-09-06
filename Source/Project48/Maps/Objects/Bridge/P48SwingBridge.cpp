#include "P48SwingBridge.h"

#include "../../Components/Layout/P48BridgeLayoutComponent.h"
#include "../../Components/Interaction/P48BridgeLoadComponent.h"
#include "../../Components/Measurement/P48MeshBoundsComponent.h"
#include "../../Components/Physics/P48BridgePhysicsComponent.h"
#include "../../Components/Rope/P48BridgeRopePathComponent.h"
#include "Components/ActorComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

AP48SwingBridge::AP48SwingBridge()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	LeftMainRopeSpline = CreateDefaultSubobject<USplineComponent>(TEXT("LeftMainRopeSpline"));
	LeftMainRopeSpline->SetupAttachment(SceneRoot);
	RightMainRopeSpline = CreateDefaultSubobject<USplineComponent>(TEXT("RightMainRopeSpline"));
	RightMainRopeSpline->SetupAttachment(SceneRoot);
	MeshBoundsComponent = CreateDefaultSubobject<UP48MeshBoundsComponent>(TEXT("MeshBoundsComponent"));
	BridgeLayoutComponent = CreateDefaultSubobject<UP48BridgeLayoutComponent>(TEXT("BridgeLayoutComponent"));
	BridgeLoadComponent = CreateDefaultSubobject<UP48BridgeLoadComponent>(TEXT("BridgeLoadComponent"));
	BridgePhysicsComponent = CreateDefaultSubobject<UP48BridgePhysicsComponent>(TEXT("BridgePhysicsComponent"));
	BridgeRopePathComponent = CreateDefaultSubobject<UP48BridgeRopePathComponent>(TEXT("BridgeRopePathComponent"));
}

void AP48SwingBridge::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildBridge();
}

void AP48SwingBridge::BeginPlay()
{
	Super::BeginPlay();
	RebuildBridge();
}

void AP48SwingBridge::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Settings.Physics.bEnableSimulation || !IsPlayerNearBridge() || !BridgePhysicsComponent->Advance(DeltaSeconds, Settings.Physics.Behavior))
	{
		return;
	}
	LoadUpdateAccumulator += DeltaSeconds;
	if (LoadUpdateAccumulator >= 0.1f)
	{
		LoadUpdateAccumulator = FMath::Fmod(LoadUpdateAccumulator, 0.1f);
		TArray<FP48BridgeLoadValue> StandingLoads;
		BridgeLoadComponent->CalculateStandingLoads(PlankComponents, StandingLoads);
		for (const FP48BridgeLoadValue& Load : StandingLoads)
		{
			BridgePhysicsComponent->AddImpulseAtLocation(Load.WorldLocation, Load.Acceleration);
		}
	}
	TArray<FTransform> PlankTransforms;
	BridgeLayoutComponent->CalculatePlankTransforms(BridgePhysicsComponent->GetNodes(), Settings, PlankTransforms);
	ApplyPlankTransforms(PlankTransforms);
	TArray<FTransform> LeftPlankLashingTransforms;
	TArray<FTransform> RightPlankLashingTransforms;
	BridgeLayoutComponent->CalculateAttachedTransforms(PlankTransforms, Settings.Assets.LeftPlankLashing, LeftPlankLashingTransforms);
	BridgeLayoutComponent->CalculateAttachedTransforms(PlankTransforms, Settings.Assets.RightPlankLashing, RightPlankLashingTransforms);
	ApplyStaticMeshTransforms(LeftPlankLashingTransforms, LeftPlankLashingComponents);
	ApplyStaticMeshTransforms(RightPlankLashingTransforms, RightPlankLashingComponents);
	FP48BridgeRopePathResult RopeResult;
	if (BridgeRopePathComponent->CalculatePaths(BridgePhysicsComponent->GetNodes(), Settings.Layout.MainRopeHeight, Settings.Collision.RopeCollisionRadius, RopeResult))
	{
		RopeCollisionUpdateAccumulator += DeltaSeconds;
		const bool bUpdateCollision = RopeCollisionUpdateAccumulator >= 0.1f;
		if (bUpdateCollision)
		{
			RopeCollisionUpdateAccumulator = FMath::Fmod(RopeCollisionUpdateAccumulator, 0.1f);
		}
		ApplyRopePaths(RopeResult, bUpdateCollision);
	}
}

void AP48SwingBridge::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AP48SwingBridge, StartLocation);
	DOREPLIFETIME(AP48SwingBridge, EndLocation);
}

void AP48SwingBridge::RebuildBridge()
{
	if (bIsRebuilding)
	{
		return;
	}
	TGuardValue<bool> RebuildGuard(bIsRebuilding, true);
	ClearGeneratedComponents();
	LoadUpdateAccumulator = 0.0f;
	RopeCollisionUpdateAccumulator = 0.0f;
	FP48MeasuredMeshBounds MeasuredBounds;
	FP48BridgeLayoutResult LayoutResult;
	if (!MeshBoundsComponent->MeasureMesh(Settings.Assets.Plank.Mesh, Settings.Assets.Plank.Scale, MeasuredBounds) || !BridgeLayoutComponent->CalculateLayout(StartLocation, EndLocation, Settings, MeasuredBounds, LayoutResult))
	{
		return;
	}
	BridgePhysicsComponent->Initialize(LayoutResult);
	PlacePlanks(LayoutResult);
	PlaceAttachedMeshes(Settings.Assets.LeftPlankLashing, LayoutResult.LeftPlankLashingTransforms, TEXT("GeneratedLeftPlankLashing"), LeftPlankLashingComponents);
	PlaceAttachedMeshes(Settings.Assets.RightPlankLashing, LayoutResult.RightPlankLashingTransforms, TEXT("GeneratedRightPlankLashing"), RightPlankLashingComponents);
	PlaceAttachedMeshes(Settings.Assets.AnchorPost, LayoutResult.AnchorPostTransforms, TEXT("GeneratedAnchorPost"), AnchorPostComponents);
	if (!Settings.PostConnection.bUsePostSockets)
	{
		PlaceAttachedMeshes(Settings.Assets.UpperPostKnot, LayoutResult.UpperPostKnotTransforms, TEXT("GeneratedUpperPostKnot"), UpperPostKnotComponents);
		PlaceAttachedMeshes(Settings.Assets.LowerPostKnot, LayoutResult.LowerPostKnotTransforms, TEXT("GeneratedLowerPostKnot"), LowerPostKnotComponents);
	}
	FString SocketError;
	if (!BridgeRopePathComponent->ConfigurePostSockets(AnchorPostComponents, Settings.PostConnection, SocketError))
	{
		UE_LOG(LogTemp, Error, TEXT("P48SwingBridge %s: %s"), *GetName(), *SocketError);
		// 기둥과 판자는 남겨 에디터에서 잘못 지정한 메시와 소켓을 확인할 수 있게 합니다.
		BridgePhysicsComponent->ResetSimulation();
		return;
	}
	FP48BridgeRopePathResult RopeResult;
	if (BridgeRopePathComponent->CalculatePaths(BridgePhysicsComponent->GetNodes(), Settings.Layout.MainRopeHeight, Settings.Collision.RopeCollisionRadius, RopeResult))
	{
		PlaceRopes(RopeResult);
	}
}

void AP48SwingBridge::ApplyBridgeImpulseAtLocation(const FVector WorldLocation, const FVector WorldImpulse)
{
	if (HasAuthority())
	{
		MulticastApplyBridgeImpulse(WorldLocation, WorldImpulse);
	}
	else
	{
		ServerApplyBridgeImpulse(WorldLocation, WorldImpulse);
	}
}

void AP48SwingBridge::ServerApplyBridgeImpulse_Implementation(const FVector WorldLocation, const FVector WorldImpulse)
{
	MulticastApplyBridgeImpulse(WorldLocation, WorldImpulse);
}

void AP48SwingBridge::MulticastApplyBridgeImpulse_Implementation(const FVector WorldLocation, const FVector WorldImpulse)
{
	BridgePhysicsComponent->AddImpulseAtLocation(WorldLocation, WorldImpulse);
}

void AP48SwingBridge::MulticastApplyBridgeImpact_Implementation(const FVector WorldLocation, const FVector NormalImpulse, const FVector OtherVelocity)
{
	BridgePhysicsComponent->AddImpactAtLocation(WorldLocation, NormalImpulse, OtherVelocity, Settings.Physics.ImpactStrength);
}

void AP48SwingBridge::OnRep_BridgeDefinition()
{
	RebuildBridge();
}

void AP48SwingBridge::HandlePlankHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, const FVector NormalImpulse, const FHitResult& Hit)
{
	if (HasAuthority() && OtherActor && OtherActor != this)
	{
		MulticastApplyBridgeImpact(Hit.ImpactPoint, NormalImpulse, OtherActor->GetVelocity());
	}
}

void AP48SwingBridge::ClearGeneratedComponents()
{
	TInlineComponentArray<UActorComponent*> Components(this);
	for (UActorComponent* Component : Components)
	{
		if (IsValid(Component) && Component != SceneRoot && Component != LeftMainRopeSpline && Component != RightMainRopeSpline && Component->ComponentHasTag(TEXT("P48GeneratedBridge")))
		{
			Component->DestroyComponent();
		}
	}
	PlankComponents.Reset();
	LeftPlankLashingComponents.Reset();
	RightPlankLashingComponents.Reset();
	AnchorPostComponents.Reset();
	UpperPostKnotComponents.Reset();
	LowerPostKnotComponents.Reset();
	LeftMainRopeComponents.Reset();
	RightMainRopeComponents.Reset();
	ConnectorRopeComponents.Reset();
	LeftLowerRopeComponents.Reset();
	RightLowerRopeComponents.Reset();
	RopeCollisionComponents.Reset();
	BridgePhysicsComponent->ResetSimulation();
	BridgeRopePathComponent->ResetPostSockets();
	LeftMainRopeSpline->ClearSplinePoints(false);
	RightMainRopeSpline->ClearSplinePoints(false);
}

void AP48SwingBridge::PlaceAttachedMeshes(const FP48BridgeAttachedMeshAssetSettings& AssetSettings, const TArray<FTransform>& Transforms, const TCHAR* NamePrefix, TArray<TObjectPtr<UStaticMeshComponent>>& OutComponents)
{
	if (!AssetSettings.Mesh)
	{
		return;
	}
	OutComponents.Reserve(Transforms.Num());
	for (int32 Index = 0; Index < Transforms.Num(); ++Index)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("%s_%d"), NamePrefix, Index));
		Component->ComponentTags.Add(TEXT("P48GeneratedBridge"));
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(AssetSettings.Mesh);
		Component->SetCollisionEnabled(AssetSettings.bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (AssetSettings.bEnableCollision) { Component->SetCollisionProfileName(TEXT("BlockAllDynamic")); }
		Component->SetupAttachment(SceneRoot);
		AddInstanceComponent(Component);
		Component->RegisterComponent();
		Component->SetWorldTransform(Transforms[Index]);
		OutComponents.Add(Component);
	}
}

void AP48SwingBridge::PlacePlanks(const FP48BridgeLayoutResult& LayoutResult)
{
	PlankComponents.Reserve(LayoutResult.PlankTransforms.Num());
	for (int32 Index = 0; Index < LayoutResult.PlankTransforms.Num(); ++Index)
	{
		UStaticMeshComponent* Plank = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("GeneratedPlank_%d"), Index));
		Plank->ComponentTags.Add(TEXT("P48GeneratedBridge"));
		Plank->SetMobility(EComponentMobility::Movable);
		Plank->SetStaticMesh(Settings.Assets.Plank.Mesh);
		Plank->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		Plank->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Plank->SetNotifyRigidBodyCollision(true);
		Plank->OnComponentHit.AddDynamic(this, &AP48SwingBridge::HandlePlankHit);
		Plank->SetupAttachment(SceneRoot);
		AddInstanceComponent(Plank);
		Plank->RegisterComponent();
		Plank->SetWorldTransform(LayoutResult.PlankTransforms[Index]);
		PlankComponents.Add(Plank);
	}
}

void AP48SwingBridge::PlaceRopes(const FP48BridgeRopePathResult& RopeResult)
{
	ApplySplinePoints(LeftMainRopeSpline, RopeResult.LeftMainPoints);
	ApplySplinePoints(RightMainRopeSpline, RopeResult.RightMainPoints);
	if (Settings.Assets.MainRope.Mesh) { CreateSplineMeshComponents(RopeResult.LeftMainSegments.Num(), TEXT("GeneratedLeftMainRope"), Settings.Assets.MainRope, LeftMainRopeComponents); }
	if (Settings.Assets.MainRope.Mesh) { CreateSplineMeshComponents(RopeResult.RightMainSegments.Num(), TEXT("GeneratedRightMainRope"), Settings.Assets.MainRope, RightMainRopeComponents); }
	if (Settings.Assets.PlankConnectorRope.Mesh) { CreateSplineMeshComponents(RopeResult.ConnectorSegments.Num(), TEXT("GeneratedConnectorRope"), Settings.Assets.PlankConnectorRope, ConnectorRopeComponents); }
	if (Settings.Assets.MainRope.Mesh)
	{
		CreateSplineMeshComponents(RopeResult.LeftLowerSegments.Num(), TEXT("GeneratedLeftLowerRope"), Settings.Assets.MainRope, LeftLowerRopeComponents);
		CreateSplineMeshComponents(RopeResult.RightLowerSegments.Num(), TEXT("GeneratedRightLowerRope"), Settings.Assets.MainRope, RightLowerRopeComponents);
	}
	if (Settings.Collision.bEnableRopeCollision)
	{
		CreateRopeCollisionComponents(RopeResult.LeftMainSegments.Num() + RopeResult.RightMainSegments.Num() + RopeResult.ConnectorSegments.Num() + RopeResult.LeftLowerSegments.Num() + RopeResult.RightLowerSegments.Num());
	}
	ApplyRopePaths(RopeResult, true);
}

void AP48SwingBridge::ApplyPlankTransforms(const TArray<FTransform>& PlankTransforms)
{
	for (int32 Index = 0; Index < PlankTransforms.Num() && PlankComponents.IsValidIndex(Index); ++Index)
	{
		PlankComponents[Index]->SetWorldTransform(PlankTransforms[Index]);
	}
}

void AP48SwingBridge::ApplyStaticMeshTransforms(const TArray<FTransform>& Transforms, const TArray<TObjectPtr<UStaticMeshComponent>>& Components)
{
	for (int32 Index = 0; Index < Transforms.Num() && Components.IsValidIndex(Index); ++Index)
	{
		Components[Index]->SetWorldTransform(Transforms[Index]);
	}
}

void AP48SwingBridge::ApplyRopePaths(const FP48BridgeRopePathResult& RopeResult, const bool bUpdateCollision)
{
	ApplySplinePoints(LeftMainRopeSpline, RopeResult.LeftMainPoints);
	ApplySplinePoints(RightMainRopeSpline, RopeResult.RightMainPoints);
	ApplyRopeSegments(RopeResult.LeftMainSegments, LeftMainRopeComponents);
	ApplyRopeSegments(RopeResult.RightMainSegments, RightMainRopeComponents);
	ApplyRopeSegments(RopeResult.ConnectorSegments, ConnectorRopeComponents);
	ApplyRopeSegments(RopeResult.LeftLowerSegments, LeftLowerRopeComponents);
	ApplyRopeSegments(RopeResult.RightLowerSegments, RightLowerRopeComponents);
	if (bUpdateCollision)
	{
		ApplyRopeCollision(RopeResult);
	}
}

bool AP48SwingBridge::IsPlayerNearBridge() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	constexpr float ActivationDistance = 6000.0f;
	const float ActivationDistanceSquared = FMath::Square(ActivationDistance);
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* PlayerController = Iterator->Get();
		const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		const FVector ClosestPoint = FMath::ClosestPointOnSegment(Pawn->GetActorLocation(), StartLocation, EndLocation);
		if (FVector::DistSquared(Pawn->GetActorLocation(), ClosestPoint) <= ActivationDistanceSquared)
		{
			return true;
		}
	}
	return false;
}

void AP48SwingBridge::ApplySplinePoints(USplineComponent* Spline, const TArray<FVector>& Points)
{
	Spline->ClearSplinePoints(false);
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		Spline->AddSplinePoint(Points[Index], ESplineCoordinateSpace::Local, false);
		Spline->SetSplinePointType(Index, ESplinePointType::Curve, false);
	}
	Spline->UpdateSpline();
}

void AP48SwingBridge::ApplyRopeSegments(const TArray<FP48BridgeRopeSegment>& Values, const TArray<TObjectPtr<USplineMeshComponent>>& Components)
{
	for (int32 Index = 0; Index < Values.Num() && Components.IsValidIndex(Index); ++Index)
	{
		Components[Index]->SetStartAndEnd(Values[Index].Start, Values[Index].StartTangent, Values[Index].End, Values[Index].EndTangent, true);
	}
}

void AP48SwingBridge::CreateSplineMeshComponents(const int32 Count, const TCHAR* NamePrefix, const FP48BridgeRopeAssetSettings& RopeAssets, TArray<TObjectPtr<USplineMeshComponent>>& OutComponents)
{
	OutComponents.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		USplineMeshComponent* Rope = NewObject<USplineMeshComponent>(this, *FString::Printf(TEXT("%s_%d"), NamePrefix, Index));
		Rope->ComponentTags.Add(TEXT("P48GeneratedBridge"));
		Rope->SetMobility(EComponentMobility::Movable);
		Rope->SetStaticMesh(RopeAssets.Mesh);
		Rope->SetForwardAxis(RopeAssets.ForwardAxis == EP48BridgeMeshAxis::X ? ESplineMeshAxis::X : ESplineMeshAxis::Y, false);
		Rope->SetStartScale(RopeAssets.CrossSectionScale, false);
		Rope->SetEndScale(RopeAssets.CrossSectionScale, false);
		Rope->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (RopeAssets.Material) { Rope->SetMaterial(0, RopeAssets.Material); }
		Rope->SetupAttachment(SceneRoot);
		AddInstanceComponent(Rope);
		Rope->RegisterComponent();
		OutComponents.Add(Rope);
	}
}

void AP48SwingBridge::CreateRopeCollisionComponents(const int32 Count)
{
	RopeCollisionComponents.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		UBoxComponent* Collision = NewObject<UBoxComponent>(this, *FString::Printf(TEXT("GeneratedRopeCollision_%d"), Index));
		Collision->ComponentTags.Add(TEXT("P48GeneratedBridge"));
		Collision->SetMobility(EComponentMobility::Movable);
		Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Collision->SetHiddenInGame(true);
		Collision->SetupAttachment(SceneRoot);
		AddInstanceComponent(Collision);
		Collision->RegisterComponent();
		RopeCollisionComponents.Add(Collision);
	}
}

void AP48SwingBridge::ApplyRopeCollision(const FP48BridgeRopePathResult& RopeResult)
{
	int32 CollisionIndex = 0;
	auto ApplyValues = [this, &CollisionIndex](const TArray<FP48BridgeRopeSegment>& Segments)
	{
		for (const FP48BridgeRopeSegment& Segment : Segments)
		{
			if (!RopeCollisionComponents.IsValidIndex(CollisionIndex))
			{
				return;
			}
			RopeCollisionComponents[CollisionIndex]->SetBoxExtent(Segment.CollisionBoxExtent, false);
			RopeCollisionComponents[CollisionIndex]->SetWorldTransform(Segment.CollisionWorldTransform);
			++CollisionIndex;
		}
	};
	ApplyValues(RopeResult.LeftMainSegments);
	ApplyValues(RopeResult.RightMainSegments);
	ApplyValues(RopeResult.ConnectorSegments);
	ApplyValues(RopeResult.LeftLowerSegments);
	ApplyValues(RopeResult.RightLowerSegments);
}
