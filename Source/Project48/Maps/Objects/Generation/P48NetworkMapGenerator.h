#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "P48NetworkMapGenerator.generated.h"

class UPCGComponent;

/** 시드와 재생성 번호를 하나의 복제 상태로 전달합니다. */
USTRUCT(BlueprintType)
struct FP48MapGenerationState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	int32 Seed = 12345;
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	int32 GenerationId = 0;
};

/** 서버가 시드를 결정하고 각 머신에서 동일 PCG 그래프를 로컬 실행합니다. */
UCLASS(Blueprintable)
class PROJECT48_API AP48NetworkMapGenerator : public AActor
{
	GENERATED_BODY()

public:
	AP48NetworkMapGenerator();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map")
	TObjectPtr<UPCGComponent> MapPCG;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	int32 InitialSeed = 12345;
	/** PIE 실행마다 서버에서 랜덤 시드를 한 번 선택합니다. 끄면 InitialSeed를 사용합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map|Debug")
	bool bRandomizeSeedOnPIEStart = true;
	UPROPERTY(ReplicatedUsing = OnRep_GenerationState, BlueprintReadOnly, Category = "Map")
	FP48MapGenerationState GenerationState;

	/** GameMode 등 서버 코드에서 호출합니다. 같은 시드로 호출해도 재생성합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Map")
	void SetMapSeed(int32 NewSeed);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnRep_GenerationState();
	int32 StartedGenerationId = 0;
};
