#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Interfaces/IHttpRequest.h"
#include "P48GameServerLifecycleSubsystem.generated.h"

// 월드 교체 중에도 종료 통지와 매치 식별 정보를 유지한다.
UCLASS()
class PROJECT48_API UP48GameServerLifecycleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	FString AcceptMatch(const FString& Options, bool bCommit = true);
	int32 GetExpectedPlayers() const { return ExpectedPlayers; }
	void EndMatch();
	void BeginReload();
	void WorldReady();

private:
	bool TickReports(float DeltaTime);
	void SendReport(const FString& Event);
	// 임시 테스트 코드: -LocalServerTest 실행 시에만 보고 서비스 없이 테스트한다. 실제 배포에서는 옵션을 제외한다.
	bool bLocalServerTest = false;
	FString ReportUrl;
	FString ReportToken;
	FGuid MatchId;
	int32 RoomId = INDEX_NONE;
	int32 ExpectedPlayers = 0;
	bool bEnding = false;
	bool bEndAcknowledged = false;
	bool bReloading = false;
	bool bWorldReady = false;
	double ReloadDeadline = 0.0;
	double NextAttempt = 0;
	FHttpRequestPtr Request;
	FTSTicker::FDelegateHandle TickerHandle;
};
