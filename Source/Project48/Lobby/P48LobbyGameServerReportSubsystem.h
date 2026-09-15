#pragma once

#include "CoreMinimal.h"
#include "HttpRequestHandler.h"
#include "HttpRouteHandle.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "P48LobbyGameServerReportSubsystem.generated.h"

class IHttpRouter;

// Receives authenticated lifecycle reports from dedicated game servers.
UCLASS()
class PROJECT48_API UP48LobbyGameServerReportSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool HandleReport(const FHttpServerRequest& Request,
		const FHttpResultCallback& OnComplete);
	void StopReceiver();

	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle RouteHandle;
	FString ReportToken;
	TSet<FString> ProcessedReports;
	uint32 ListenPort = 0;
};
