#include "P48GameplayMessageTags.h"

namespace P48GameplayTags
{
	namespace Match
	{
		UE_DEFINE_GAMEPLAY_TAG(PlayerCountChanged, "Event.Match.PlayerCount.Changed")
	}

	namespace Character
	{
		UE_DEFINE_GAMEPLAY_TAG(DeathRequested, "Event.Character.Death.Requested")
	}

	namespace Weapon
	{
		UE_DEFINE_GAMEPLAY_TAG(HitOccurred, "Event.Weapon.Hit.Occurred")
	}

	namespace Map
	{
		UE_DEFINE_GAMEPLAY_TAG(GenerationRequested, "Event.Map.Generation.Requested")
		UE_DEFINE_GAMEPLAY_TAG(GenerationCompleted, "Event.Map.Generation.Completed")
	}
}
