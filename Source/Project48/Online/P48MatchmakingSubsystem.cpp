#include "P48MatchmakingSubsystem.h"

#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"

void UP48MatchmakingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SessionInterface = Online::GetSessionInterface(GetWorld());
	if (SessionInterface.IsValid() == false)
	{
		return;
	}
}

void UP48MatchmakingSubsystem::Deinitialize()
{
	if (SessionInterface.IsValid())
	{
		if (CreateSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(
				CreateSessionCompleteDelegateHandle);
		}

		if (DestroySessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(
				DestroySessionCompleteDelegateHandle);
		}
	}

	SessionInterface.Reset();
	Super::Deinitialize();
}

bool UP48MatchmakingSubsystem::CreateSession(int32 NumPublicConnections, bool bIsLANMatch)
{
	if (SessionInterface.IsValid() == false)
	{
		return false;
	}

	if (NumPublicConnections <= 0)
	{
		return false;
	}

	if (CreateSessionCompleteDelegateHandle.IsValid()
		|| DestroySessionCompleteDelegateHandle.IsValid())
	{
		return false;
	}

	if (HasActiveSession())
	{
		return false;
	}

	FOnlineSessionSettings SessionSettings;
	SessionSettings.NumPublicConnections = NumPublicConnections;
	SessionSettings.bIsLANMatch = bIsLANMatch;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.bIsDedicated = IsRunningDedicatedServer();

	if (const UWorld* World = GetWorld())
	{
		SessionSettings.Set(
			SETTING_MAPNAME,
			World->GetMapName(),
			EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}

	CreateSessionCompleteDelegateHandle =
		SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
			FOnCreateSessionCompleteDelegate::CreateUObject(
				this,
				&ThisClass::HandleCreateSessionComplete));

	if (SessionInterface->CreateSession(0, NAME_GameSession, SessionSettings) == false)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(
			CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
		return false;
	}

	return true;
}

bool UP48MatchmakingSubsystem::DestroySession()
{
	if (SessionInterface.IsValid() == false)
	{
		return false;
	}

	if (CreateSessionCompleteDelegateHandle.IsValid()
		|| DestroySessionCompleteDelegateHandle.IsValid())
	{
		return false;
	}

	if (HasActiveSession() == false)
	{
		return false;
	}

	DestroySessionCompleteDelegateHandle =
		SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(
				this,
				&ThisClass::HandleDestroySessionComplete));

	if (SessionInterface->DestroySession(NAME_GameSession) == false)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(
			DestroySessionCompleteDelegateHandle);
		DestroySessionCompleteDelegateHandle.Reset();
		return false;
	}

	return true;
}

bool UP48MatchmakingSubsystem::HasActiveSession() const
{
	return SessionInterface.IsValid()
		&& SessionInterface->GetNamedSession(NAME_GameSession) != nullptr;
}

void UP48MatchmakingSubsystem::HandleCreateSessionComplete(
	FName,
	bool)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(
			CreateSessionCompleteDelegateHandle);
	}
	CreateSessionCompleteDelegateHandle.Reset();
}

void UP48MatchmakingSubsystem::HandleDestroySessionComplete(
	FName,
	bool)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(
			DestroySessionCompleteDelegateHandle);
	}
	DestroySessionCompleteDelegateHandle.Reset();
}
