#include "P48GameplayMessageLibrary.h"

bool UP48GameplayMessageLibrary::BroadcastEvent(
	const UObject* WorldContextObject,
	const FGameplayTag Channel)
{
	return Broadcast(WorldContextObject, Channel, FP48EmptyMessage());
}

bool UP48GameplayMessageLibrary::StopListening(FGameplayMessageListenerHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return false;
	}

	Handle.Unregister();
	Handle = FGameplayMessageListenerHandle();
	return true;
}
