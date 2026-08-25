// P48GameModeBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "P48GameModeBase.generated.h"

/**
 * @brief 서버 전용 판정과 게임 진행을 담당하는 클래스
 */
UCLASS()
class PROJECT48_API AP48GameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	virtual void OnPostLogin(AController* NewPlayer) override;
};
