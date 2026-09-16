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
	ClearDestroySessionTimeout();
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
	DestroySessionPurpose = EP48DestroySessionPurpose::None;
	PendingDestroySessionName = NAME_None;
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

	return BeginDestroySession(
		NAME_GameSession, EP48DestroySessionPurpose::UserLeave);
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
	FName SessionName,
	bool bWasSuccessful)
{
	const EP48DestroySessionPurpose CompletedPurpose = DestroySessionPurpose;
	const FName CompletedSessionName = PendingDestroySessionName.IsNone()
		? SessionName : PendingDestroySessionName;
	ClearDestroySessionTimeout();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(
			DestroySessionCompleteDelegateHandle);
	}
	DestroySessionCompleteDelegateHandle.Reset();
	ForceRemoveNamedSession(CompletedSessionName);
	const bool bSessionRemoved = !SessionInterface.IsValid()
		|| SessionInterface->GetNamedSession(CompletedSessionName) == nullptr;
	DestroySessionPurpose = EP48DestroySessionPurpose::None;
	PendingDestroySessionName = NAME_None;

	if (CompletedPurpose == EP48DestroySessionPurpose::JoinFailureCleanup)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Matchmaking] Failed Join session cleanup completed. BackendSuccess=%d Removed=%d"),
			bWasSuccessful, bSessionRemoved);
		OnJoinSessionCompleted.Broadcast(false);
	}
	else if (CompletedPurpose == EP48DestroySessionPurpose::UserLeave)
	{
		OnLeaveSessionCompleted.Broadcast(bWasSuccessful && bSessionRemoved);
	}
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

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		if (SessionInterface.IsValid()
			&& SessionInterface->GetNamedSession(SessionName) != nullptr)
		{
			CleanupJoinedSessionAfterTravelFailure(
				SessionName, TEXT("Join callback returned a failure with a Named Session present"));
			return;
		}
		OnJoinSessionCompleted.Broadcast(false);
		return;
	}

	FString ConnectString;
	if (!SessionInterface.IsValid()
		|| !SessionInterface->GetResolvedConnectString(SessionName, ConnectString)
		|| ConnectString.TrimStartAndEnd().IsEmpty())
	{
		CleanupJoinedSessionAfterTravelFailure(
			SessionName, TEXT("Connect string resolution failed"));
		return;
	}

	APlayerController* PlayerController =
		GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		CleanupJoinedSessionAfterTravelFailure(
			SessionName, TEXT("Local PlayerController was not available"));
		return;
	}

	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
	OnJoinSessionCompleted.Broadcast(true);
}

bool UP48MatchmakingSubsystem::BeginDestroySession(
	FName SessionName,
	const EP48DestroySessionPurpose Purpose)
{
	if (!SessionInterface.IsValid() || Purpose == EP48DestroySessionPurpose::None)
	{
		return false;
	}
	if (SessionName.IsNone()) SessionName = NAME_GameSession;
	DestroySessionPurpose = Purpose;
	PendingDestroySessionName = SessionName;
	DestroySessionCompleteDelegateHandle =
		SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(
				this, &ThisClass::HandleDestroySessionComplete));

	if (!SessionInterface->DestroySession(SessionName))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(
			DestroySessionCompleteDelegateHandle);
		DestroySessionCompleteDelegateHandle.Reset();
		ForceRemoveNamedSession(SessionName);
		DestroySessionPurpose = EP48DestroySessionPurpose::None;
		PendingDestroySessionName = NAME_None;
		UE_LOG(LogTemp, Error,
			TEXT("[Matchmaking] DestroySession could not start. Session=%s Purpose=%d"),
			*SessionName.ToString(), static_cast<int32>(Purpose));
		if (Purpose == EP48DestroySessionPurpose::JoinFailureCleanup)
		{
			OnJoinSessionCompleted.Broadcast(false);
		}
		return false;
	}
	// 일부 OnlineSubsystem은 Destroy 완료 콜백을 호출 스택 안에서 즉시 실행할 수 있다.
	// 그 경우 완료 처리 후 고아 watchdog을 등록하지 않는다.
	if (DestroySessionPurpose != Purpose
		|| !DestroySessionCompleteDelegateHandle.IsValid())
	{
		return true;
	}

	DestroySessionTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::HandleDestroySessionTimeout),
		10.0f);
	return true;
}

void UP48MatchmakingSubsystem::CleanupJoinedSessionAfterTravelFailure(
	FName SessionName,
	const TCHAR* Reason)
{
	if (SessionName.IsNone()) SessionName = NAME_GameSession;
	UE_LOG(LogTemp, Error,
		TEXT("[Matchmaking] Join succeeded but travel preparation failed: %s. Session=%s"),
		Reason ? Reason : TEXT("Unknown"), *SessionName.ToString());
	if (!SessionInterface.IsValid()
		|| SessionInterface->GetNamedSession(SessionName) == nullptr)
	{
		OnJoinSessionCompleted.Broadcast(false);
		return;
	}
	BeginDestroySession(SessionName, EP48DestroySessionPurpose::JoinFailureCleanup);
}

bool UP48MatchmakingSubsystem::HandleDestroySessionTimeout(float)
{
	DestroySessionTimeoutHandle.Reset();
	const EP48DestroySessionPurpose TimedOutPurpose = DestroySessionPurpose;
	const FName TimedOutSessionName = PendingDestroySessionName.IsNone()
		? NAME_GameSession : PendingDestroySessionName;
	if (SessionInterface.IsValid() && DestroySessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(
			DestroySessionCompleteDelegateHandle);
	}
	DestroySessionCompleteDelegateHandle.Reset();
	ForceRemoveNamedSession(TimedOutSessionName);
	DestroySessionPurpose = EP48DestroySessionPurpose::None;
	PendingDestroySessionName = NAME_None;
	UE_LOG(LogTemp, Error,
		TEXT("[Matchmaking] DestroySession timed out; removed local Named Session. Session=%s"),
		*TimedOutSessionName.ToString());
	if (TimedOutPurpose == EP48DestroySessionPurpose::JoinFailureCleanup)
	{
		OnJoinSessionCompleted.Broadcast(false);
	}
	else if (TimedOutPurpose == EP48DestroySessionPurpose::UserLeave)
	{
		OnLeaveSessionCompleted.Broadcast(false);
	}
	return false;
}

void UP48MatchmakingSubsystem::ClearDestroySessionTimeout()
{
	if (DestroySessionTimeoutHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DestroySessionTimeoutHandle);
	}
	DestroySessionTimeoutHandle.Reset();
}

void UP48MatchmakingSubsystem::ForceRemoveNamedSession(const FName SessionName)
{
	if (SessionInterface.IsValid()
		&& SessionInterface->GetNamedSession(SessionName) != nullptr)
	{
		SessionInterface->RemoveNamedSession(SessionName);
	}
}

bool UP48MatchmakingSubsystem::IsSessionOperationInProgress() const
{
	return CreateSessionCompleteDelegateHandle.IsValid()
		|| DestroySessionCompleteDelegateHandle.IsValid()
		|| FindSessionsCompleteDelegateHandle.IsValid()
		|| JoinSessionCompleteDelegateHandle.IsValid();
}
