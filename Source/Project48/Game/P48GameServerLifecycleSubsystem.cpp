#include "P48GameServerLifecycleSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

void UP48GameServerLifecycleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// 임시 테스트 코드: bat에서 명시적으로 전달한 옵션으로만 HTTP 보고 생략을 활성화한다.
	bLocalServerTest = FParse::Param(FCommandLine::Get(), TEXT("LocalServerTest"));
	if (bLocalServerTest)
	{
		UE_LOG(LogTemp, Display, TEXT("[GameServer] LocalServerTest enabled: HTTP reporting is disabled."));
	}
	FParse::Value(FCommandLine::Get(), TEXT("GameServerReportUrl="), ReportUrl);
	FParse::Value(FCommandLine::Get(), TEXT("GameServerReportToken="), ReportToken);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickReports), 1.0f);
}

void UP48GameServerLifecycleSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	if (Request)
	{
		Request->OnProcessRequestComplete().Unbind();
		Request->CancelRequest();
		Request.Reset();
	}
	Super::Deinitialize();
}

FString UP48GameServerLifecycleSubsystem::AcceptMatch(const FString& Options, bool bCommit)
{
	if (bEnding) return TEXT("Game server is resetting.");
	const FString MatchOption = UGameplayStatics::ParseOption(Options, TEXT("LobbyMatchId"));
	// 로비를 거치지 않는 기존 로컬 테스트는 유지한다.
	if (!UGameplayStatics::HasOption(Options, TEXT("LobbyMatchId"))
		&& !UGameplayStatics::HasOption(Options, TEXT("LobbyRoomId")) && !MatchId.IsValid()) return FString();
	FGuid IncomingMatch;
	int32 IncomingRoom = INDEX_NONE;
	int32 IncomingCount = 0;
	const FString CountOption = UGameplayStatics::ParseOption(Options, TEXT("ExpectedPlayers"));
	if (!FGuid::Parse(MatchOption, IncomingMatch) || !IncomingMatch.IsValid()
		|| !LexTryParseString(IncomingRoom, *UGameplayStatics::ParseOption(Options, TEXT("LobbyRoomId")))
		|| IncomingRoom < 0
		|| (!CountOption.IsEmpty() && (!LexTryParseString(IncomingCount, *CountOption) || IncomingCount < 2)))
	{
		return TEXT("Invalid lobby match options.");
	}
	if (MatchId.IsValid())
	{
		return IncomingMatch == MatchId && IncomingRoom == RoomId
			&& (CountOption.IsEmpty() || IncomingCount == ExpectedPlayers)
			? FString() : TEXT("Lobby match options do not match this server.");
	}
	if (IncomingCount < 2) return TEXT("Initial ExpectedPlayers is required.");
	// 임시 테스트 코드: 로컬 테스트만 보고 설정 검사를 생략한다. 매치 정보와 참가 인원 검증은 유지한다.
	if (!bLocalServerTest && (ReportUrl.IsEmpty() || ReportToken.IsEmpty())) return TEXT("Game server report endpoint is not configured.");
	if (bCommit)
	{
		MatchId = IncomingMatch;
		RoomId = IncomingRoom;
		ExpectedPlayers = IncomingCount;
	}
	return FString();
}

void UP48GameServerLifecycleSubsystem::EndMatch()
{
	if (bEnding) return;
	bEnding = true;
	bEndAcknowledged = !MatchId.IsValid();
	NextAttempt = 0;
	TickReports(0);
}

void UP48GameServerLifecycleSubsystem::WorldReady()
{
	if (bReloading)
	{
		bReloading = false;
		bWorldReady = true;
	}
}

bool UP48GameServerLifecycleSubsystem::TickReports(float DeltaTime)
{
	// 임시 테스트 코드: HTTP 보고/재시도 없이 맵 재로딩 완료 후에만 다음 매치를 받도록 초기화한다.
	// 실제 보고 서비스의 매치 종료/서버 준비 통지는 이 모드에서 테스트하지 않는다.
	if (bLocalServerTest)
	{
		if (bEnding && bWorldReady)
		{
			MatchId.Invalidate();
			RoomId = INDEX_NONE;
			ExpectedPlayers = 0;
			bEnding = bEndAcknowledged = bWorldReady = false;
			NextAttempt = 0;
			UE_LOG(LogTemp, Display, TEXT("[GameServer] LocalServerTest: reset complete; ready for next match."));
		}
		return true;
	}
	if (!bEnding || Request || FPlatformTime::Seconds() < NextAttempt) return true;
	if (!bEndAcknowledged) SendReport(TEXT("match_ended"));
	else if (bWorldReady)
	{
		if (MatchId.IsValid()) SendReport(TEXT("server_ready"));
		else bEnding = bWorldReady = false;
	}
	return true;
}

void UP48GameServerLifecycleSubsystem::SendReport(const FString& Event)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("roomId"), RoomId);
	Body->SetStringField(TEXT("matchId"), MatchId.ToString(EGuidFormats::Digits));
	Body->SetStringField(TEXT("event"), Event);
	FString Json;
	FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&Json));
	Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(ReportUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + ReportToken);
	Request->SetContentAsString(Json);
	Request->SetTimeout(10.0f);
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[this, Event](FHttpRequestPtr Completed, FHttpResponsePtr Response, bool bSuccess)
		{
			if (Request != Completed) return;
			Request.Reset();
			const int32 Code = Response ? Response->GetResponseCode() : 0;
			if (!bSuccess || Code < 200 || Code >= 300)
			{
				UE_LOG(LogTemp, Warning, TEXT("[GameServer] %s failed (%d); retry in 5 seconds."), *Event, Code);
				NextAttempt = FPlatformTime::Seconds() + 5.0;
				return;
			}
			UE_LOG(LogTemp, Log, TEXT("[GameServer] %s acknowledged. Match=%s"), *Event, *MatchId.ToString());
			if (Event == TEXT("match_ended")) bEndAcknowledged = true;
			else
			{
				MatchId.Invalidate();
				RoomId = INDEX_NONE;
				ExpectedPlayers = 0;
				bEnding = bEndAcknowledged = bWorldReady = false;
			}
		});
	const FHttpRequestPtr ActiveRequest = Request;
	if (!ActiveRequest->ProcessRequest() && Request == ActiveRequest)
	{
		Request.Reset();
		NextAttempt = FPlatformTime::Seconds() + 5.0;
		UE_LOG(LogTemp, Warning, TEXT("[GameServer] Could not send %s; retry in 5 seconds."), *Event);
	}
}
