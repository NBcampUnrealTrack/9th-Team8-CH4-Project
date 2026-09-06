#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "P48PCGSeedWorldSubsystem.generated.h"

class UPCGComponent;

/** PIE에 복사된 에디터 생성 결과도 네트워크 시드로 갱신합니다. */
UCLASS()
class UP48PCGSeedWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void OnWorldBeginPlay(UWorld& World) override;
protected:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override { return Type == EWorldType::Game || Type == EWorldType::PIE; }
private:
	TArray<TWeakObjectPtr<UPCGComponent>> PendingComponents;

	void CleanupGraphs();
	void GenerateGraphs();
};
