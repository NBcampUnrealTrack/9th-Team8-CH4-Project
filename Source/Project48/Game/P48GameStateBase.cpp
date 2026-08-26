// P48GameStateBase.cpp


#include "P48GameStateBase.h"
#include "Net/UnrealNetwork.h"

void AP48GameStateBase::BeginPlay()
{
	Super::BeginPlay();

	// GameState 확인용 로그 출력
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[%s] P48GameStateBase, MatchPhase: %s"),
		HasAuthority() ? TEXT("Server") : TEXT("Client"),
		*UEnum::GetValueAsString(MatchPhase));
}

void AP48GameStateBase::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AP48GameStateBase, MatchPhase);
}

void AP48GameStateBase::SetMatchPhase(EP48MatchPhase NewMatchPhase)
{
	if (HasAuthority() == false)
	{
		return;
	}
	
	MatchPhase = NewMatchPhase;
}