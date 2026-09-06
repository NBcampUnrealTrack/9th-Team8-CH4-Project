#pragma once

#include "CoreMinimal.h"
#include "../../Datas/Structs/P48SwingBridgeSettings.h"
#include "Components/ActorComponent.h"
#include "P48BridgePhysicsComponent.generated.h"

struct FP48BridgeLayoutResult;

/** 다리 노드 상태를 소유하고 위치 값만 계산합니다. 메시를 생성하거나 이동하지 않습니다. */
UCLASS(ClassGroup = (P48), meta = (BlueprintSpawnableComponent))
class PROJECT48_API UP48BridgePhysicsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UP48BridgePhysicsComponent();

	void Initialize(const FP48BridgeLayoutResult& LayoutResult);
	void ResetSimulation();
	bool Advance(float DeltaSeconds, EP48BridgeBehavior Behavior);
	void AddImpulseAtLocation(const FVector& WorldLocation, const FVector& WorldImpulse);
	void AddImpactAtLocation(const FVector& WorldLocation, const FVector& NormalImpulse, const FVector& OtherVelocity, float ImpactStrength);
	const TArray<FP48BridgePlankNode>& GetNodes() const { return Nodes; }
	bool HasNodes() const { return Nodes.Num() >= 2; }

private:
	void SimulateStep(float FixedDeltaTime, EP48BridgeBehavior Behavior);
	void SolveDistance(FVector& A, FVector& B, float RestDistance, float InverseMassA, float InverseMassB) const;

	TArray<FP48BridgePlankNode> Nodes;
	float TimeAccumulator = 0.0f;
};
