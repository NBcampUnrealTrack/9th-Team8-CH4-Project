#pragma once

#include "CoreMinimal.h"
#include "Project48/Character/P48PlayerState.h"
#include "HSPlayerState.generated.h"

UCLASS()
class PROJECT48_API AHSPlayerState : public AP48PlayerState
{
	GENERATED_BODY()
	
public:
	/*AHSPlayerState();

	UPROPERTY(Replicated, BlueprintReadOnly)
	FString UserID;
	
	UPROPERTY(ReplicatedUsing = OnRep_Nickname)
	FString Nickname;

	UFUNCTION()
	void OnRep_Nickname();

	const FString& GetNickname() const { return Nickname; }

	void SetNickname(const FString& InNickname);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;*/
	
};
