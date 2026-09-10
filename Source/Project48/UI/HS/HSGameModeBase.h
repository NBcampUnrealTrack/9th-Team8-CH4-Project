#pragma once

#include "CoreMinimal.h"
#include "Project48/Game/P48GameModeBase.h"
#include "HSGameModeBase.generated.h"

UCLASS()
class PROJECT48_API AHSGameModeBase : public AP48GameModeBase
{
	GENERATED_BODY()
	
	virtual void PreLogin(
		const FString& Options,
		const FString& Address,
		const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;
};
