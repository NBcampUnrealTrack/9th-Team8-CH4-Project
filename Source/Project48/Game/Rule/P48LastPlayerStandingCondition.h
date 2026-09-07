// P48LastPlayerStandingCondition.h

#pragma once

#include "CoreMinimal.h"
#include "P48RoundWinCondition.h"
#include "P48LastPlayerStandingCondition.generated.h"

/**
 * @brief 마지막 생존자를 라운드 승자로 판정하는 규칙
 */
UCLASS()
class PROJECT48_API UP48LastPlayerStandingCondition : public UP48RoundWinCondition
{
	GENERATED_BODY()

public:
	virtual FP48RoundResult Evaluate(const TArray<AP48PlayerState*>& Participants) const override;
};
