#include "HSPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Project48/Character/P48PlayerCharacter.h"
#include "Project48/UI/P48PlayerNameWidgetComponent.h"

/*AHSPlayerState::AHSPlayerState()
{
	bReplicates = true;
}

void AHSPlayerState::SetNickname(const FString& InNickname)
{
	if (HasAuthority() == false) return;

	Nickname = InNickname;
}

void AHSPlayerState::OnRep_Nickname()
{
	// 클라이언트에서 닉네임이 변경되었을 때 호출
	AP48PlayerCharacter* P48Character = Cast<AP48PlayerCharacter>(GetPawn());

	if (IsValid(P48Character) == false) return;

	UP48PlayerNameWidgetComponent* Nameplate = P48Character->FindComponentByClass<UP48PlayerNameWidgetComponent>();

	if (IsValid(Nameplate) == false) return;

	Nameplate->UpdateNickname();
}

void AHSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHSPlayerState, Nickname);
}*/