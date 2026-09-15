#include "P48LobbyGameServerReportSubsystem.h"

#include "P48LobbyGameMode.h"

#include "Dom/JsonObject.h"
#include "HttpPath.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogP48LobbyGameServerReport, Log, All);

namespace P48LobbyGameServerReport
{
	const FHttpPath RoutePath(TEXT("/game-server/report"));

	TUniquePtr<FHttpServerResponse> MakeResponse(
		EHttpServerResponseCodes Code, const FString& Message)
	{
		TUniquePtr<FHttpServerResponse> Response =
			FHttpServerResponse::Create(Message, TEXT("text/plain; charset=utf-8"));
		Response->Code = Code;
		return Response;
	}

	const TArray<FString>* FindHeader(
		const FHttpServerRequest& Request, const FString& Name)
	{
		for (const TPair<FString, TArray<FString>>& Header : Request.Headers)
		{
			if (Header.Key.Equals(Name, ESearchCase::IgnoreCase))
			{
				return &Header.Value;
			}
		}
		return nullptr;
	}
}

void UP48LobbyGameServerReportSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!IsRunningDedicatedServer()) return;

	int32 ConfiguredPort = 0;
	if (!FParse::Value(FCommandLine::Get(),
		TEXT("LobbyGameServerReportPort="), ConfiguredPort))
	{
		return;
	}
	FParse::Value(FCommandLine::Get(), TEXT("GameServerReportToken="), ReportToken);
	if (ConfiguredPort < 1 || ConfiguredPort > 65535 || ReportToken.IsEmpty())
	{
		UE_LOG(LogP48LobbyGameServerReport, Error,
			TEXT("Game server report receiver requires a valid port and token."));
		return;
	}

	ListenPort = static_cast<uint32>(ConfiguredPort);
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	// Enabling listeners first makes GetHttpRouter report a bind failure instead
	// of leaving an unusable router that looks successfully configured.
	HttpServerModule.StartAllListeners();
	Router = HttpServerModule.GetHttpRouter(ListenPort, true);
	if (!Router)
	{
		UE_LOG(LogP48LobbyGameServerReport, Error,
			TEXT("Could not create HTTP router on port %u."), ListenPort);
		ListenPort = 0;
		return;
	}

	RouteHandle = Router->BindRoute(
		P48LobbyGameServerReport::RoutePath,
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(
			this, &ThisClass::HandleReport));
	if (!RouteHandle.IsValid())
	{
		UE_LOG(LogP48LobbyGameServerReport, Error,
			TEXT("Could not bind game server report route."));
		StopReceiver();
		return;
	}

	UE_LOG(LogP48LobbyGameServerReport, Display,
		TEXT("Listening for game server reports on port %u at %s."),
		ListenPort, *P48LobbyGameServerReport::RoutePath.GetPath());
}

void UP48LobbyGameServerReportSubsystem::Deinitialize()
{
	StopReceiver();
	ProcessedReports.Reset();
	ReportToken.Reset();
	Super::Deinitialize();
}

bool UP48LobbyGameServerReportSubsystem::HandleReport(
	const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	using namespace P48LobbyGameServerReport;

	const TArray<FString>* Authorization = FindHeader(Request, TEXT("Authorization"));
	const FString ExpectedAuthorization = TEXT("Bearer ") + ReportToken;
	if (!Authorization || Authorization->Num() != 1
		|| (*Authorization)[0] != ExpectedAuthorization)
	{
		OnComplete(MakeResponse(EHttpServerResponseCodes::Denied,
			TEXT("Unauthorized")));
		return true;
	}

	if (Request.Body.IsEmpty() || Request.Body.Num() > 4096)
	{
		OnComplete(MakeResponse(EHttpServerResponseCodes::BadRequest,
			TEXT("Request body must contain at most 4096 bytes")));
		return true;
	}

	const FUTF8ToTCHAR BodyConverter(
		reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
	const FString Body(BodyConverter.Length(), BodyConverter.Get());
	TSharedPtr<FJsonObject> Json;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
	{
		OnComplete(MakeResponse(EHttpServerResponseCodes::BadRequest,
			TEXT("Invalid JSON")));
		return true;
	}

	double RoomNumber = 0.0;
	FString MatchIdText;
	FString Event;
	FGuid MatchId;
	if (!Json->TryGetNumberField(TEXT("roomId"), RoomNumber)
		|| RoomNumber < 1.0 || RoomNumber > MAX_int32
		|| !FMath::IsNearlyEqual(RoomNumber, FMath::RoundToDouble(RoomNumber))
		|| !Json->TryGetStringField(TEXT("matchId"), MatchIdText)
		|| !FGuid::Parse(MatchIdText, MatchId) || !MatchId.IsValid()
		|| !Json->TryGetStringField(TEXT("event"), Event)
		|| (Event != TEXT("match_ended") && Event != TEXT("server_ready")))
	{
		OnComplete(MakeResponse(EHttpServerResponseCodes::BadRequest,
			TEXT("Invalid game server report")));
		return true;
	}

	const int32 RoomId = static_cast<int32>(RoomNumber);
	const FString ReportKey = MatchId.ToString(EGuidFormats::Digits)
		+ TEXT(":") + Event;
	if (ProcessedReports.Contains(ReportKey))
	{
		OnComplete(MakeResponse(EHttpServerResponseCodes::Ok,
			TEXT("Already processed")));
		return true;
	}

	UWorld* World = GetWorld();
	AP48LobbyGameMode* LobbyMode = World
		? World->GetAuthGameMode<AP48LobbyGameMode>() : nullptr;
	if (!IsValid(LobbyMode))
	{
		OnComplete(MakeResponse(EHttpServerResponseCodes::ServiceUnavail,
			TEXT("Lobby game mode is unavailable")));
		return true;
	}

	const bool bAccepted = Event == TEXT("match_ended")
		? LobbyMode->ReportGameSessionEnded(RoomId, MatchId)
		: LobbyMode->ReportGameServerReady(RoomId, MatchId);
	if (!bAccepted)
	{
		OnComplete(MakeResponse(EHttpServerResponseCodes::Conflict,
			TEXT("Report does not match an active lobby assignment")));
		return true;
	}

	ProcessedReports.Add(ReportKey);
	UE_LOG(LogP48LobbyGameServerReport, Log,
		TEXT("Accepted %s. Room=%d Match=%s"),
		*Event, RoomId, *MatchId.ToString(EGuidFormats::Digits));
	OnComplete(MakeResponse(EHttpServerResponseCodes::Ok, TEXT("Accepted")));
	return true;
}

void UP48LobbyGameServerReportSubsystem::StopReceiver()
{
	if (Router && RouteHandle.IsValid())
	{
		Router->UnbindRoute(RouteHandle);
	}
	RouteHandle.Reset();
	Router.Reset();
	ListenPort = 0;
}
