#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "P48GameInstance.generated.h"

UCLASS()
class PROJECT48_API UP48GameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	void SetUserID(const FString& InUserID) { UserID = InUserID; }
	void SetNickname(const FString& InNickname) { Nickname = InNickname; }

	FString GetUserID() const { return UserID; }
	FString GetNickname() const { return Nickname; }

private:
	UPROPERTY()
	FString UserID;

	UPROPERTY()
	FString Nickname;
};
