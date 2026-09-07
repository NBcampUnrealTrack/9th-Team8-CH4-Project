#pragma once

#include "CoreMinimal.h"

/** P48 맵 생성 노드와 Unreal Spawner 노드 사이의 공통 Attribute 계약입니다. */
namespace P48PCGSpawnAttributeNames
{
	inline const FName Mesh(TEXT("Mesh"));
	inline const FName ActorClass(TEXT("ActorClass"));
	inline const FName IslandIndex(TEXT("IslandIndex"));
	inline const FName SurfaceNormal(TEXT("SurfaceNormal"));
	inline const FName SurfaceOutward(TEXT("SurfaceOutward"));
	inline const FName PlacementRadius(TEXT("PlacementRadius"));
	inline const FName CanSpawnPlayer(TEXT("CanSpawnPlayer"));
	inline const FName SurfaceLocation(TEXT("SurfaceLocation"));
	inline const FName SpawnSlotIndex(TEXT("SpawnSlotIndex"));
	inline const FName GenerationId(TEXT("GenerationId"));
	inline const FName StartLocation(TEXT("StartLocation"));
	inline const FName EndLocation(TEXT("EndLocation"));
}
