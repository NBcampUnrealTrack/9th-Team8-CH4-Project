#include "P48MatchmakingSubsystem.h"

#include "GameFramework/PlayerController.h"
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

		if (FindSessionsCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(
				FindSessionsCompleteDelegateHandle);
		}

		if (JoinSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(
				JoinSessionCompleteDelegateHandle);
		}
	}

	SessionSearch.Reset();
	SessionSearchResults.Reset();
	bJoinFirstResultAfterSearch = false;
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

	if (IsSessionOperationInProgress())
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
	SessionSettings.bIsDedicated = GetWorld()
		&& GetWorld()->GetNetMode() == NM_DedicatedServer;

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

	if (IsSessionOperationInProgress())
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

bool UP48MatchmakingSubsystem::FindSessions(int32 MaxSearchResults, bool bIsLANQuery)
{
	if (SessionInterface.IsValid() == false || MaxSearchResults <= 0)
	{
		return false;
	}

	if (IsSessionOperationInProgress())
	{
		return false;
	}

	SessionSearchResults.Reset();
	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = MaxSearchResults;
	SessionSearch->bIsLanQuery = bIsLANQuery;

	FindSessionsCompleteDelegateHandle =
		SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
			FOnFindSessionsCompleteDelegate::CreateUObject(
				this,
				&ThisClass::HandleFindSessionsComplete));

	if (SessionInterface->FindSessions(0, SessionSearch.ToSharedRef()) == false)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(
			FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
		SessionSearch.Reset();
		return false;
	}

	return true;
}

bool UP48MatchmakingSubsystem::FindAndJoinFirstSession(
	int32 MaxSearchResults,
	bool bIsLANQuery)
{
	bJoinFirstResultAfterSearch = true;
	if (FindSessions(MaxSearchResults, bIsLANQuery))
	{
		return true;
	}

	bJoinFirstResultAfterSearch = false;
	return false;
}

bool UP48MatchmakingSubsystem::JoinSession(int32 SearchResultIndex)
{
	if (SessionInterface.IsValid() == false || SessionSearch.IsValid() == false)
	{
		return false;
	}

	if (IsSessionOperationInProgress() || HasActiveSession())
	{
		return false;
	}

	if (SessionSearch->SearchResults.IsValidIndex(SearchResultIndex) == false)
	{
		return false;
	}

	JoinSessionCompleteDelegateHandle =
		SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
			FOnJoinSessionCompleteDelegate::CreateUObject(
				this,
				&ThisClass::HandleJoinSessionComplete));

	if (SessionInterface->JoinSession(
			0,
			NAME_GameSession,
			SessionSearch->SearchResults[SearchResultIndex]) == false)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(
			JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
		return false;
	}

	return true;
}

bool UP48MatchmakingSubsystem::LeaveSession()
{
	return DestroySession();
}

TArray<FP48SessionSearchResult> UP48MatchmakingSubsystem::GetSessionSearchResults() const
{
	return SessionSearchResults;
}

void UP48MatchmakingSubsystem::HandleCreateSessionComplete(
	FName,
	bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(
			CreateSessionCompleteDelegateHandle);
	}
	CreateSessionCompleteDelegateHandle.Reset();
	OnCreateSessionCompleted.Broadcast(bWasSuccessful);
}

void UP48MatchmakingSubsystem::HandleDestroySessionComplete(
	FName,
	bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(
			DestroySessionCompleteDelegateHandle);
	}
	DestroySessionCompleteDelegateHandle.Reset();
	OnLeaveSessionCompleted.Broadcast(bWasSuccessful);
}

void UP48MatchmakingSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(
			FindSessionsCompleteDelegateHandle);
	}
	FindSessionsCompleteDelegateHandle.Reset();
	SessionSearchResults.Reset();

	if (bWasSuccessful && SessionSearch.IsValid())
	{
		for (int32 Index = 0; Index < SessionSearch->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& OnlineResult =
				SessionSearch->SearchResults[Index];

			FP48SessionSearchResult Result;
			Result.ResultIndex = Index;
			Result.RoomName = OnlineResult.Session.OwningUserName;
			if (Result.RoomName.IsEmpty())
			{
				Result.RoomName = FString::Printf(TEXT("Session %d"), Index + 1);
			}

			OnlineResult.Session.SessionSettings.Get(SETTING_MAPNAME, Result.MapName);
			Result.MaxPlayers = OnlineResult.Session.SessionSettings.NumPublicConnections;
			Result.CurrentPlayers = FMath::Max(
				0,
				Result.MaxPlayers - OnlineResult.Session.NumOpenPublicConnections);
			Result.PingInMs = OnlineResult.PingInMs;
			SessionSearchResults.Add(MoveTemp(Result));
		}
	}

	const bool bShouldJoinFirstResult = bJoinFirstResultAfterSearch;
	bJoinFirstResultAfterSearch = false;

	OnFindSessionsCompleted.Broadcast(bWasSuccessful, SessionSearchResults);

	if (bShouldJoinFirstResult)
	{
		if (bWasSuccessful && SessionSearchResults.IsEmpty() == false)
		{
			if (JoinSession(0) == false)
			{
				OnJoinSessionCompleted.Broadcast(false);
			}
		}
		else
		{
			OnJoinSessionCompleted.Broadcast(false);
		}
	}
}

void UP48MatchmakingSubsystem::HandleJoinSessionComplete(
	FName SessionName,
	EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(
			JoinSessionCompleteDelegateHandle);
	}
	JoinSessionCompleteDelegateHandle.Reset();

	bool bWasSuccessful = Result == EOnJoinSessionCompleteResult::Success;
	FString ConnectString;
	if (bWasSuccessful)
	{
		bWasSuccessful = SessionInterface.IsValid()
			&& SessionInterface->GetResolvedConnectString(SessionName, ConnectString);
	}

	if (bWasSuccessful)
	{
		APlayerController* PlayerController =
			GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (IsValid(PlayerController))
		{
			PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
		}
		else
		{
			bWasSuccessful = false;
		}
	}

	OnJoinSessionCompleted.Broadcast(bWasSuccessful);
}

bool UP48MatchmakingSubsystem::IsSessionOperationInProgress() const
{
	return CreateSessionCompleteDelegateHandle.IsValid()
		|| DestroySessionCompleteDelegateHandle.IsValid()
		|| FindSessionsCompleteDelegateHandle.IsValid()
		|| JoinSessionCompleteDelegateHandle.IsValid();
}
