// P48PlayerState.cpp


#include "P48PlayerState.h"

#include "Net/UnrealNetwork.h"

void AP48PlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AP48PlayerState, bIsReady);
	DOREPLIFETIME(AP48PlayerState, bIsAlive);
	DOREPLIFETIME(AP48PlayerState, bIsMatchParticipant);
	DOREPLIFETIME(AP48PlayerState, RoundWinCount);
}

void AP48PlayerState::SetReady(bool bNewReady)
{
	if (HasAuthority() == false)
	{
		return;
	}
	
	if (bIsReady == bNewReady)
	{
		return;
	}
	
	bIsReady = bNewReady;
	
	UE_LOG(LogTemp,Warning,TEXT("[Server] %s Ready: %s"),*GetPlayerName(),
		bIsReady ? TEXT("true") : TEXT("false"));
	
}

void AP48PlayerState::SetAlive(bool bNewAlive)
{
	if (HasAuthority() == false)
	{
		return;
	}

	if (bIsAlive == bNewAlive)
	{
		return;
	}

	bIsAlive = bNewAlive;

	UE_LOG(LogTemp,Warning,TEXT("[Server] %s Alive: %s"),*GetPlayerName(),
		bIsAlive ? TEXT("true") : TEXT("false"));
}

void AP48PlayerState::SetMatchParticipant(bool bNewParticipant)
{
	if (HasAuthority() == false)
	{
		return;
	}

	if (bIsMatchParticipant == bNewParticipant)
	{
		return;
	}

	bIsMatchParticipant = bNewParticipant;

	UE_LOG(LogTemp,Warning,TEXT("[Server] %s MatchParticipant: %s"),*GetPlayerName(),
		bIsMatchParticipant ? TEXT("true") : TEXT("false"));
}

void AP48PlayerState::AddRoundWin()
{
	if (HasAuthority() == false)
	{
		return;
	}

	RoundWinCount++;

	UE_LOG(LogTemp,Warning,TEXT("[Server] %s RoundWinCount: %d"),*GetPlayerName(),RoundWinCount);
}

void AP48PlayerState::ResetForNewRound()
{
	if (HasAuthority() == false)
	{
		return;
	}

	SetAlive(bIsMatchParticipant);

	UE_LOG(LogTemp,Warning,TEXT("[Server] %s reset for new round"),*GetPlayerName());
}

void AP48PlayerState::ResetForNewMatch()
{
	if (HasAuthority() == false)
	{
		return;
	}

	SetReady(false);
	SetAlive(false);
	SetMatchParticipant(false);

	RoundWinCount = 0;

	UE_LOG(LogTemp,Warning,TEXT("[Server] %s reset for new match"),*GetPlayerName());
}