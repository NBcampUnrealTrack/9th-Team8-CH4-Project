#include "P48SaveGameSubsystem.h"

#include "P48PlayerSaveGame.h"
#include "Kismet/GameplayStatics.h"

const FString UP48SaveGameSubsystem::PlayerSaveSlotName = TEXT("PlayerProfile");

void UP48SaveGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	LoadOrCreatePlayerSave();
}

bool UP48SaveGameSubsystem::SavePlayerData(const FString& Nickname, int32 PlayerLevel, int32 Experience)
{
	if (!CurrentSaveGame)
	{
		LoadOrCreatePlayerSave();
	}
	
	if (!CurrentSaveGame)
	{
		UE_LOG(LogTemp, Error, TEXT("SaveGame 객체를 생성하지 못했습니다."));
		
		return false;
	}
	
	CurrentSaveGame->Nickname = Nickname.IsEmpty() ? TEXT("Player") : Nickname;
	
	CurrentSaveGame->PlayerLevel = FMath::Max(PlayerLevel, 1);
	
	CurrentSaveGame->Experience = FMath::Max(Experience, 0);
	
	const bool bSaveSucceeded = UGameplayStatics::SaveGameToSlot(
		CurrentSaveGame,
		PlayerSaveSlotName,
		PlayerSaveUserIndex);
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Player Save %s " "[Nickname=%s, Level=%d, Experience=%d]"),
		bSaveSucceeded ? TEXT("성공") : TEXT("실패"),
		*CurrentSaveGame->Nickname,
		CurrentSaveGame->PlayerLevel,
		CurrentSaveGame->Experience);
	
	return bSaveSucceeded;
}

UP48PlayerSaveGame* UP48SaveGameSubsystem::LoadOrCreatePlayerSave()
{
	// 세이브 파일 있을 때 불러옴
	if (DoesPlayerSaveExist())
	{
		USaveGame* LoadedObject =
			UGameplayStatics::LoadGameFromSlot(PlayerSaveSlotName, PlayerSaveUserIndex);
		
		CurrentSaveGame = Cast<UP48PlayerSaveGame>(LoadedObject);
		
		if (CurrentSaveGame)
		{
			UE_LOG(
				LogTemp,
				Log,
				TEXT("Player Save 로드 성공" "[Nickname=%s, Level=%d, Experience=%d]"),
				*CurrentSaveGame->Nickname,
				CurrentSaveGame->PlayerLevel,
				CurrentSaveGame->Experience);
			
			return CurrentSaveGame;
		}
		
		UE_LOG(LogTemp, Warning, TEXT("저장 파일 형식이 올바르지 않습니다."));
	}
	
	// 세이브 파일이 없거나 로드 실패 시 객체 생성
	CurrentSaveGame = Cast<UP48PlayerSaveGame>(UGameplayStatics::CreateSaveGameObject(UP48PlayerSaveGame::StaticClass()));
	
	if (!CurrentSaveGame)
	{
		UE_LOG(LogTemp, Error, TEXT("기본 SaveGame 생성에 실패했습니다."));
		
		return nullptr;
	}
	
	const bool bSaveSucceeded = UGameplayStatics::SaveGameToSlot(
		CurrentSaveGame,
		PlayerSaveSlotName,
		PlayerSaveUserIndex
		);
	
	if (bSaveSucceeded)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("기본 Player Save 생성 [Nickname=%s, Level=%d, Experience=%d]"),
			*CurrentSaveGame->Nickname,
			CurrentSaveGame->PlayerLevel,
			CurrentSaveGame->Experience
			);	
	}
	else
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("기본 Player Save 실패 [Nickname=%s, Level=%d, Experience=%d]"),
			*CurrentSaveGame->Nickname,
			CurrentSaveGame->PlayerLevel,
			CurrentSaveGame->Experience
			);	
	}
	
	return CurrentSaveGame;
}

UP48PlayerSaveGame* UP48SaveGameSubsystem::GetCurrentPlayerSave() const
{
	return CurrentSaveGame;
}

bool UP48SaveGameSubsystem::DoesPlayerSaveExist() const
{
	return UGameplayStatics::DoesSaveGameExist(
		PlayerSaveSlotName,
		PlayerSaveUserIndex);
}
