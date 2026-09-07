#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "P48PlayerSaveGame.generated.h"

UCLASS(BlueprintType)
class PROJECT48_API UP48PlayerSaveGame : public USaveGame
{
	GENERATED_BODY()
	
public:
	UP48PlayerSaveGame();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, Category = "SaveGame|Version")
	int32 SaveVersion;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, Category = "SaveGame|Player")
	FString Nickname;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, Category = "SaveGame|Player")
	int32 PlayerLevel;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, SaveGame, Category = "SaveGame|Player")
	int32 Experience;
};
