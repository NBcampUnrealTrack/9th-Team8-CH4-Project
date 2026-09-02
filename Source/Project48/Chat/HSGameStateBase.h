// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Project48/Game/P48GameStateBase.h"
#include "ChatMessageData.h"
#include "HSGameStateBase.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT48_API AHSGameStateBase : public AP48GameStateBase
{
	GENERATED_BODY()
	
public:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastReceiveChatMessage(const FChatMessage& InChatMessage);
};
