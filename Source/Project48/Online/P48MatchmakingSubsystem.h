#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "P48MatchmakingSubsystem.generated.h"

class FOnlineSessionSearch;

USTRUCT(BlueprintType)
struct FP48SessionSearchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Online|Session")
	int32 ResultIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Online|Session")
	FString RoomName;

	UPROPERTY(BlueprintReadOnly, Category = "Online|Session")
	FString MapName;

	UPROPERTY(BlueprintReadOnly, Category = "Online|Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Online|Session")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Online|Session")
	int32 PingInMs = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FP48SessionOperationComplete,
	bool,
	bWasSuccessful);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FP48FindSessionsComplete,
	bool,
	bWasSuccessful,
	const TArray<FP48SessionSearchResult>&,
	SearchResults);

UCLASS()
class PROJECT48_API UP48MatchmakingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Online|Session")
	bool CreateSession(int32 NumPublicConnections = 8, bool bIsLANMatch = true);

	bool DestroySession();

	UFUNCTION(BlueprintPure, Category = "Online|Session")
	bool HasActiveSession() const;

	UFUNCTION(BlueprintCallable, Category = "Online|Session")
	bool FindSessions(int32 MaxSearchResults = 100, bool bIsLANQuery = true);

	UFUNCTION(BlueprintCallable, Category = "Online|Session")
	bool JoinSession(int32 SearchResultIndex);

	UFUNCTION(BlueprintCallable, Category = "Online|Session")
	bool LeaveSession();

	UFUNCTION(BlueprintPure, Category = "Online|Session")
	TArray<FP48SessionSearchResult> GetSessionSearchResults() const;

	UPROPERTY(BlueprintAssignable, Category = "Online|Session")
	FP48SessionOperationComplete OnCreateSessionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Online|Session")
	FP48FindSessionsComplete OnFindSessionsCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Online|Session")
	FP48SessionOperationComplete OnJoinSessionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Online|Session")
	FP48SessionOperationComplete OnLeaveSessionCompleted;

private:
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(
		FName SessionName,
		EOnJoinSessionCompleteResult::Type Result);
	bool IsSessionOperationInProgress() const;

	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	TArray<FP48SessionSearchResult> SessionSearchResults;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FDelegateHandle DestroySessionCompleteDelegateHandle;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
};
