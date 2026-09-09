#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "P48GameplayMessageLibrary.generated.h"

UCLASS()
class PROJECT48_API UP48GameplayMessageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template <typename TPayload>
	static bool Broadcast(
		const UObject* WorldContextObject,
		const FGameplayTag Channel,
		const TPayload& Payload)
	{
		if (!IsValid(WorldContextObject) || !Channel.IsValid())
		{
			return false;
		}

		if (!UGameplayMessageSubsystem::HasInstance(WorldContextObject))
		{
			return false;
		}

		UGameplayMessageSubsystem::Get(WorldContextObject).BroadcastMessage(Channel, Payload);
		return true;
	}

	template <typename TListener, typename TPayload>
	static FGameplayMessageListenerHandle Listen(
		TListener* Listener,
		const FGameplayTag Channel,
		void (TListener::*Handler)(FGameplayTag, const TPayload&),
		const EGameplayMessageMatch MatchType = EGameplayMessageMatch::ExactMatch)
	{
		if (!IsValid(Listener) || !Channel.IsValid() || Handler == nullptr)
		{
			return FGameplayMessageListenerHandle();
		}

		if (!UGameplayMessageSubsystem::HasInstance(Listener))
		{
			return FGameplayMessageListenerHandle();
		}

		FGameplayMessageListenerParams<TPayload> Params;
		Params.MatchType = MatchType;
		Params.SetMessageReceivedCallback(Listener, Handler);

		return UGameplayMessageSubsystem::Get(Listener).RegisterListener<TPayload>(Channel, Params);
	}

	static bool StopListening(FGameplayMessageListenerHandle& Handle);
};
