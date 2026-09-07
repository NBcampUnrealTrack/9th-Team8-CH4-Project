#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "P48PlayerStart.generated.h"

class AP48PCGSeedState;

/** PCG가 서버에 배치하는 하늘섬 전용 PlayerStart입니다. */
UCLASS(Blueprintable)
class PROJECT48_API AP48PlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|PlayerSpawn", meta = (ExposeOnSpawn = "true"))
	int32 SpawnSlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|PlayerSpawn", meta = (ExposeOnSpawn = "true"))
	int32 IslandIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|PlayerSpawn", meta = (ExposeOnSpawn = "true"))
	int32 GenerationId = 0;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<AP48PCGSeedState> RegisteredSeedState;
	int32 RegistrationAttempts = 0;

	void RegisterWithSeedState();
};
