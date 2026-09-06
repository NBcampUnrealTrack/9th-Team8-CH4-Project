#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "P48MeshBoundsComponent.generated.h"

class UStaticMesh;

/** 메시 Bounds를 스케일까지 반영해 측정한 결과입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48MeasuredMeshBounds
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map|Measurement", meta = (Units = "cm"))
	FVector LocalOrigin = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map|Measurement", meta = (Units = "cm"))
	FVector ScaledSize = FVector::ZeroVector;

	bool IsValid() const { return ScaledSize.X > UE_KINDA_SMALL_NUMBER && ScaledSize.Y > UE_KINDA_SMALL_NUMBER && ScaledSize.Z > UE_KINDA_SMALL_NUMBER; }
};

/** StaticMesh의 로컬 Bounds 크기를 측정하는 역할만 담당합니다. */
UCLASS(ClassGroup = (P48), meta = (BlueprintSpawnableComponent))
class PROJECT48_API UP48MeshBoundsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UP48MeshBoundsComponent();

	UFUNCTION(BlueprintPure, Category = "Map|Measurement")
	bool MeasureMesh(UStaticMesh* Mesh, FVector MeshScale, FP48MeasuredMeshBounds& OutBounds) const;
};
