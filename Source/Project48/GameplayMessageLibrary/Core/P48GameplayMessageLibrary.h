#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "P48GameplayMessagePayloads.h"
#include "P48GameplayMessageLibrary.generated.h"

UCLASS()
class PROJECT48_API UP48GameplayMessageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Payload 없이 채널의 사건 발생만 알립니다. */
	UFUNCTION(BlueprintCallable, Category = "Gameplay Message", meta = (WorldContext = "WorldContextObject"))
	static bool BroadcastEvent(
		const UObject* WorldContextObject,
		FGameplayTag Channel);

	/** Local GameInstance dispatch only. True does not acknowledge a listener or retain the payload. */
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

	/** Payload를 받지 않는 멤버 함수로 단순 이벤트 채널을 구독합니다. */
	template <typename TListener>
	static FGameplayMessageListenerHandle ListenEvent(
		TListener* Listener,
		const FGameplayTag Channel,
		void (TListener::*Handler)(FGameplayTag),
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

		TWeakObjectPtr<TListener> WeakListener(Listener);
		return UGameplayMessageSubsystem::Get(Listener).RegisterListener<FP48EmptyMessage>(
			Channel,
			[WeakListener, Handler](const FGameplayTag ActualChannel, const FP48EmptyMessage&)
			{
				if (TListener* StrongListener = WeakListener.Get())
				{
					(StrongListener->*Handler)(ActualChannel);
				}
			},
			MatchType);
	}

	static bool StopListening(FGameplayMessageListenerHandle& Handle);
};
