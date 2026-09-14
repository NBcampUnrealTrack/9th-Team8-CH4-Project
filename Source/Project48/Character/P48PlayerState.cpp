// P48PlayerState.cpp


#include "P48PlayerState.h"

#include "P48PlayerCharacter.h"
#include "Project48/Game/P48SurvivalGameMode.h"

#include "Net/UnrealNetwork.h"
#include "Project48/UI/P48PlayerNameWidgetComponent.h"

void AP48PlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AP48PlayerState, bIsReady);
	DOREPLIFETIME(AP48PlayerState, bIsAlive);
	DOREPLIFETIME(AP48PlayerState, bIsMatchParticipant);
	DOREPLIFETIME(AP48PlayerState, RoundWinCount);
	DOREPLIFETIME(AP48PlayerState, StunCount);
	DOREPLIFETIME(AP48PlayerState, bHasWeapon);
	DOREPLIFETIME(AP48PlayerState, Nickname);
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
	ResetStunCount();
	
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

void AP48PlayerState::AddStunCount(int32 Amount)
{
	if (!HasAuthority())
	{
		return;
	}
	StunCount += Amount;
}

void AP48PlayerState::ResetStunCount()
{
	if (!HasAuthority())
	{
		return;
	}
	StunCount = 0;
}

void AP48PlayerState::OnDeath()
{
	if (!HasAuthority())
	{
		return;
	}
	
	if (AP48SurvivalGameMode* SurvivalGM = GetWorld()->GetAuthGameMode<AP48SurvivalGameMode>())
	{
		SurvivalGM->NotifyPlayerEliminated(this);
		UE_LOG(LogTemp, Error, TEXT("[Server]: [%s] is died."), *GetName());
	}
}

void AP48PlayerState::SetHasWeapon(bool NewHasWeapon)
{
	if (!HasAuthority())
	{
		return;
	}
	bHasWeapon = NewHasWeapon;
}

void AP48PlayerState::ResetHasWeapon()
{
	if (!HasAuthority())
	{
		return;
	}
	bHasWeapon = false;
}

/* UI */
void AP48PlayerState::SetNickname(const FString& InNickname)
{
	if (HasAuthority() == false) return;

	Nickname = InNickname;
}

void AP48PlayerState::OnRep_Nickname()
{
	// 클라이언트에서 닉네임이 변경되었을 때 호출
	AP48PlayerCharacter* P48Character = Cast<AP48PlayerCharacter>(GetPawn());

	if (IsValid(P48Character) == false) return;

	UP48PlayerNameWidgetComponent* Nameplate = P48Character->FindComponentByClass<UP48PlayerNameWidgetComponent>();

	if (IsValid(Nameplate) == false) return;

	Nameplate->UpdateNickname();
}