#include "P48SaveGameSubsystem.h"

#include "P48PlayerSaveGame.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogP48SaveGame, Log, All);

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
		UE_LOG(LogP48SaveGame, Error, TEXT("SaveGame 객체를 생성하지 못했습니다."));
		
		return false;
	}
	
	ApplyValidatedPlayerData(Nickname, PlayerLevel, Experience);
	
	const bool bSaveSucceeded = SaveCurrentPlayerSave();
	
	UE_LOG(
		LogP48SaveGame,
		Log,
		TEXT("Player Save %s " "[Nickname=%s, Level=%d, Experience=%d]"),
		bSaveSucceeded ? TEXT("성공") : TEXT("실패"),
		*CurrentSaveGame->Nickname,
		CurrentSaveGame->PlayerLevel,
		CurrentSaveGame->Experience);
	
	return bSaveSucceeded;
}

void UP48SaveGameSubsystem::ApplyValidatedPlayerData(
	const FString& Nickname, int32 PlayerLevel, int32 Experience)
{
	if (!CurrentSaveGame)
	{
		return;
	}
	
	CurrentSaveGame->Nickname = Nickname.IsEmpty() ? TEXT("Player") : Nickname;
	CurrentSaveGame->PlayerLevel = FMath::Max(PlayerLevel, 1);
	CurrentSaveGame->Experience = FMath::Max(Experience, 0);
}

bool UP48SaveGameSubsystem::SaveCurrentPlayerSave() const
{
	if (!CurrentSaveGame)
	{
		return false;
	}
	
	return UGameplayStatics::SaveGameToSlot(
		CurrentSaveGame,
		PlayerSaveSlotName,
		PlayerSaveUserIndex);
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
			const FString LoadedNickname = CurrentSaveGame->Nickname;
			const int32 LoadedPlayerLevel = CurrentSaveGame->PlayerLevel;
			const int32 LoadedExperience = CurrentSaveGame->Experience;
			
			ApplyValidatedPlayerData(LoadedNickname, LoadedPlayerLevel, LoadedExperience);
			
			const bool bDataWasCorrected =
				LoadedNickname != CurrentSaveGame->Nickname ||
				LoadedPlayerLevel != CurrentSaveGame->PlayerLevel ||
				LoadedExperience != CurrentSaveGame->Experience;
			
			if (bDataWasCorrected)
			{
				const bool bCorrectionSaved = SaveCurrentPlayerSave();
				
				if (bCorrectionSaved)
				{
					UE_LOG(
						LogP48SaveGame,
						Warning,
						TEXT("Player Save의 잘못된 값을 보정하고 저장했습니다."));
				}
				else
				{
					UE_LOG(
					LogP48SaveGame,
					Error,
					TEXT("Player Save 값은 보정했지만 파일 저장에 실패했습니다."));
				}
			}
			
			UE_LOG(
				LogP48SaveGame,
				Log,
				TEXT("Player Save 로드 성공" "[Nickname=%s, Level=%d, Experience=%d]"),
				*CurrentSaveGame->Nickname,
				CurrentSaveGame->PlayerLevel,
				CurrentSaveGame->Experience);
			
			return CurrentSaveGame;
		}
		
		UE_LOG(LogP48SaveGame, Warning, TEXT("저장 파일 형식이 올바르지 않습니다."));
	}
	
	// 세이브 파일이 없거나 로드 실패 시 객체 생성
	CurrentSaveGame = Cast<UP48PlayerSaveGame>(UGameplayStatics::CreateSaveGameObject(UP48PlayerSaveGame::StaticClass()));
	
	if (!CurrentSaveGame)
	{
		UE_LOG(LogP48SaveGame, Error, TEXT("기본 SaveGame 생성에 실패했습니다."));
		
		return nullptr;
	}
	
	const bool bSaveSucceeded = SaveCurrentPlayerSave();
	
	if (bSaveSucceeded)
	{
		UE_LOG(
			LogP48SaveGame,
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
			LogP48SaveGame,
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
