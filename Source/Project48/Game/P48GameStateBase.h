// P48GameStateBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "P48GameStateBase.generated.h"

/**
 * 
 */
UENUM(BlueprintType)
enum class EP48MatchPhase : uint8
{
	Waiting,
	Countdown,
	Playing,
	RoundEnd,
	MatchEnd
};

/**
 * @brief 모든 클라이언트가 알아야 하는 공용 상태를 담당하는 클래스
 */
UCLASS()
class PROJECT48_API AP48GameStateBase : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	void SetMatchPhase(EP48MatchPhase NewMatchPhase);
	
	UPROPERTY(Replicated, BlueprintReadOnly)
	EP48MatchPhase MatchPhase = EP48MatchPhase::Waiting;
};
