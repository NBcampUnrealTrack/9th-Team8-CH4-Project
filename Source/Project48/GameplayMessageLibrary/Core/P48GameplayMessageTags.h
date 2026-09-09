#pragma once

#include "NativeGameplayTags.h"

namespace P48GameplayTags
{
	namespace Match
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(PlayerCountChanged)
	}

	namespace Character
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(DeathRequested)
	}

	namespace Weapon
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitOccurred)
	}

	namespace Map
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(GenerationRequested)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(GenerationCompleted)
	}
}
