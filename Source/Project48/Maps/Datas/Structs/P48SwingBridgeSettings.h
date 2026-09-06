#pragma once

#include "CoreMinimal.h"
#include "P48SwingBridgeSettings.generated.h"

class UStaticMesh;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EP48BridgeMeshAxis : uint8
{
	X,
	Y
};

UENUM(BlueprintType)
enum class EP48BridgeBehavior : uint8
{
	Tight,
	Normal,
	Loose
};

/** 다리 바닥을 구성하는 판자 에셋입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48BridgePlankAssetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	TObjectPtr<UStaticMesh> Mesh = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	EP48BridgeMeshAxis ForwardAxis = EP48BridgeMeshAxis::Y;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset", meta = (ClampMin = "0.001", AllowPreserveRatio = "true"))
	FVector Scale = FVector::OneVector;
};

/** 한 종류의 로프 역할에 사용할 메시 표시 설정입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48BridgeRopeAssetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	TObjectPtr<UStaticMesh> Mesh = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	TObjectPtr<UMaterialInterface> Material = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	EP48BridgeMeshAxis ForwardAxis = EP48BridgeMeshAxis::X;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset", meta = (ClampMin = "0.001"))
	FVector2D CrossSectionScale = FVector2D::UnitVector;
};

/** 계산된 기준 Transform에 부착할 조립용 메시 설정입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48BridgeAttachedMeshAssetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	TObjectPtr<UStaticMesh> Mesh = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset", meta = (ClampMin = "0.001", AllowPreserveRatio = "true"))
	FVector Scale = FVector::OneVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset", meta = (Units = "cm"))
	FVector LocationOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	FRotator RotationOffset = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	bool bEnableCollision = false;
};

/** 조립 역할별 에셋을 서로 독립적으로 지정합니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48SwingBridgeAssets
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	FP48BridgePlankAssetSettings Plank;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	FP48BridgeAttachedMeshAssetSettings LeftPlankLashing;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	FP48BridgeAttachedMeshAssetSettings RightPlankLashing;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	FP48BridgeRopeAssetSettings MainRope;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	FP48BridgeRopeAssetSettings PlankConnectorRope;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	FP48BridgeAttachedMeshAssetSettings AnchorPost;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	FP48BridgeAttachedMeshAssetSettings UpperPostKnot;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Asset")
	FP48BridgeAttachedMeshAssetSettings LowerPostKnot;
};

/** 길이에 맞춰 판자를 배치하는 규칙입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48SwingBridgeLayoutSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Layout", meta = (ClampMin = "0.0", Units = "cm"))
	float PlankGap = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Layout", meta = (ClampMin = "0.0", Units = "cm"))
	float InitialSag = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Layout", meta = (ClampMin = "0.0", Units = "cm"))
	float EndInset = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Layout", meta = (ClampMin = "0.0", Units = "cm"))
	float MainRopeHeight = 120.0f;
};

/** 복잡한 solver 수치 대신 다리의 성격만 선택합니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48SwingBridgePhysicsSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Physics")
	bool bEnableSimulation = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Physics")
	EP48BridgeBehavior Behavior = EP48BridgeBehavior::Normal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Physics", meta = (ClampMin = "0.0"))
	float ImpactStrength = 0.02f;
};

/** 로프와 플레이어 충돌에 필요한 최소 설정입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48SwingBridgeCollisionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Collision")
	bool bEnableRopeCollision = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Collision", meta = (ClampMin = "1.0", Units = "cm"))
	float RopeCollisionRadius = 4.0f;
};

/** 매듭이 포함된 통합 기둥 메시의 로프 연결 설정입니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48BridgePostConnectionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|PostConnection")
	bool bUsePostSockets = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|PostConnection", meta = (EditCondition = "bUsePostSockets"))
	FName UpperSocketName = TEXT("UpperRopeAnchor");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|PostConnection", meta = (EditCondition = "bUsePostSockets"))
	bool bGenerateLowerRopes = true;
	/** 하단 로프 표시 여부와 관계없이 판자 높이 기준으로 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|PostConnection", meta = (EditCondition = "bUsePostSockets"))
	FName LowerSocketName = TEXT("LowerRopeAnchor");
	/** 기둥 Bounds와 판자 사이의 측면 및 입구 여유입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|PostConnection", meta = (EditCondition = "bUsePostSockets", ClampMin = "0.0", Units = "cm"))
	float PostClearance = 10.0f;
};

/** 액터에는 이 설정 하나만 노출하고 세부 항목은 역할별로 접어 관리합니다. */
USTRUCT(BlueprintType)
struct PROJECT48_API FP48SwingBridgeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	FP48SwingBridgeAssets Assets;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	FP48SwingBridgeLayoutSettings Layout;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	FP48SwingBridgePhysicsSettings Physics;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	FP48SwingBridgeCollisionSettings Collision;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
	FP48BridgePostConnectionSettings PostConnection;
};

/** 판자 한 장의 좌우 끝을 나타내는 런타임 배치 결과입니다. */
struct FP48BridgePlankNode
{
	FVector RestLeft = FVector::ZeroVector;
	FVector RestRight = FVector::ZeroVector;
	FVector CurrentLeft = FVector::ZeroVector;
	FVector CurrentRight = FVector::ZeroVector;
	FVector PreviousLeft = FVector::ZeroVector;
	FVector PreviousRight = FVector::ZeroVector;
	FVector AccumulatedLeftAcceleration = FVector::ZeroVector;
	FVector AccumulatedRightAcceleration = FVector::ZeroVector;
	bool bFixed = false;
};
