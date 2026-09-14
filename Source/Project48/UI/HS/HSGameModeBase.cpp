#include "HSGameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Project48/Character/P48PlayerState.h"
#include "Project48/Database/P48NicknameDatabaseSubsystem.h"
#include "Project48/Game/P48GameInstance.h"

void AHSGameModeBase::PreLogin(
		const FString& Options,
		const FString& Address,
		const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}
	
	// 접속 옵션에서 UserID 추출
	const FString UserID = UGameplayStatics::ParseOption(Options, TEXT("UserID")).TrimStartAndEnd();
	if (UserID.IsEmpty())
	{
		ErrorMessage = TEXT("UserID 없습니다.");
		return;
	}
	const FString Nickname = UGameplayStatics::ParseOption(Options, TEXT("Nickname")).TrimStartAndEnd();
	
	if (Nickname.IsEmpty())
	{
		ErrorMessage = TEXT("Nickname 없습니다.");
		return;
	}
	
	UP48GameInstance* P48GameInstance = Cast<UP48GameInstance>(GetGameInstance());
	if (IsValid(P48GameInstance) == false)
	{
		ErrorMessage = TEXT("GameInstance를 가져올 수 없습니다.");
		return;
	}
	
	UP48NicknameDatabaseSubsystem* DB = P48GameInstance->GetSubsystem<UP48NicknameDatabaseSubsystem>();
	if (IsValid(DB) == false)
	{
		ErrorMessage = TEXT("Nickname Database를 가져올 수 없습니다.");
		return;
	}
	
	FString StoredNickname;
	
	const EP48NicknameLookupResult LookupResult =
		DB->FindNicknameByUserID(UserID, StoredNickname);
	
	if (LookupResult == EP48NicknameLookupResult::Success)
	{
		AGameStateBase* CurrentGameState = GetWorld()->GetGameState<AGameStateBase>();
		
		if (IsValid(CurrentGameState))
		{
			for (APlayerState* ConnectedPlayerState : CurrentGameState->PlayerArray)
			{
				const AP48PlayerState* P48PlayerState = Cast<AP48PlayerState>(ConnectedPlayerState);
				
				if (!IsValid(P48PlayerState))
				{
					continue;
				}
				
				if (P48PlayerState->GetNickname().Equals(StoredNickname, ESearchCase::IgnoreCase))
				{
					ErrorMessage = TEXT("UserAlreadyRegistered");
					
					return;
				}
			}
		}
		
		P48GameInstance->SetNickname(StoredNickname);
		P48GameInstance->SetUserID(UserID);
		
		UE_LOG(LogTemp, Log, TEXT("기존 사용자 로그인 성공 - UserID: %s / Nickname: %s"), *UserID, *StoredNickname);
		
		return;
	}
	
	if (LookupResult != EP48NicknameLookupResult::NotFound)
	{
		ErrorMessage =
			LookupResult == EP48NicknameLookupResult::InvalidUserID ? TEXT("InvalidUserID") : TEXT("default");
		
		return;
	}
	
	EP48NicknameRegistrationResult Result = DB->TryRegisterNickname(UserID, Nickname);
	
	switch (Result)
	{
	case EP48NicknameRegistrationResult::Success:
		P48GameInstance->SetNickname(Nickname);
		P48GameInstance->SetUserID(UserID);
		
		UE_LOG(LogTemp, Log, TEXT("신규 사용자 로그인 성공 - UserID: %s / Nickname: %s"), *UserID, *Nickname);
		break;
		
	case EP48NicknameRegistrationResult::DuplicateNickname:
		ErrorMessage = TEXT("DuplicateNickname");
		break;

	case EP48NicknameRegistrationResult::InvalidFormat:
		ErrorMessage = TEXT("InvalidFormat");
		break;

	case EP48NicknameRegistrationResult::UserAlreadyRegistered:
		ErrorMessage = TEXT("UserAlreadyRegistered");
		break;

	default:
		ErrorMessage = TEXT("default");
		break;
	}
}

void AHSGameModeBase::PostLogin(APlayerController* NewPC)
{
	Super::PostLogin(NewPC);
	
	if (IsValid(NewPC) == false) return;
	
	AP48PlayerState* PS = NewPC->GetPlayerState<AP48PlayerState>();
	if (IsValid(PS) == false) return;

	UP48GameInstance* P48GI = Cast<UP48GameInstance>(GetGameInstance());
	if (IsValid(P48GI) == false) return;
	
	PS->SetNickname(P48GI->GetNickname());
}
