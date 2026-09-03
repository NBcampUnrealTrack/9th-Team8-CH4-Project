#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "P48SaveGameSubsystem.generated.h"

class UP48PlayerSaveGame;

UCLASS()
class PROJECT48_API UP48SaveGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	UFUNCTION(BlueprintCallable, Category = "SaveGame")
	bool SavePlayerData(const FString& Nickname, int32 PlayerLevel, int32 Experience);
	
	UFUNCTION(BlueprintCallable, Category = "SaveGame")
	UP48PlayerSaveGame* LoadOrCreatePlayerSave();
	
	UFUNCTION(BlueprintPure, Category = "SaveGame")
	UP48PlayerSaveGame* GetCurrentPlayerSave() const;
	
	UFUNCTION(BlueprintPure, Category = "SaveGame")
	bool DoesPlayerSaveExist() const;
	
private:
	static const FString PlayerSaveSlotName;
	static constexpr int32 PlayerSaveUserIndex = 0;
	
	UPROPERTY()
	TObjectPtr<UP48PlayerSaveGame> CurrentSaveGame;
};
