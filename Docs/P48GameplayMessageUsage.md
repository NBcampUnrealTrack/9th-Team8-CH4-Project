# Maps Gameplay Message 사용 가이드

## 1. 이번 단계의 범위

맵 생성과 플레이어 수 확정을 서로 독립적으로 처리합니다. 맵은 Maps가 World 시작 시 자체 Seed로 생성하고, GameMode는 확정된 플레이어 수만 별도로 알립니다.

- 송신 채널: `P48GameplayTags::Match::PlayerCountChanged`
- 송신 Payload: `FP48MatchPlayerCountMessage`
- 수신 위치: `UP48PCGSeedWorldSubsystem`
- 수신 값: `PlayerCount`만
- Seed: `P48 Network Seed`가 서버에서 생성하고 `SeedState`가 클라이언트에 복제
- 실제 GameMode 송신 코드는 이 단계에서 수정하지 않습니다.

현재 수신 흐름은 다음과 같습니다.

```text
Maps World 시작
    └─ RequestGeneration()
          └─ P48 Network Seed가 서버 Seed 생성
                └─ SeedState 복제 후 섬·다리 생성
                      └─ PlayerSpawnSelector는 PlayerCount 대기

GameMode 또는 매치 시스템
    └─ PlayerCountChanged Broadcast(PlayerCount)
          └─ UP48PCGSeedWorldSubsystem::HandlePlayerCountChanged
                └─ 대기 중인 PlayerSpawnSelector가 PlayerStart 생성 재개
```

`GameplayMessageSubsystem`의 메시지는 같은 `UWorld` 안에서 전달되며 네트워크 RPC를 대신하지 않습니다. 서버 GameMode의 Broadcast는 대기 중인 PlayerStart 분기만 재개하고 섬·다리 맵을 다시 생성하지 않습니다.

## 2. 필요한 헤더

```cpp
#include "Project48/GameplayMessageLibrary/Core/P48GameplayMessageLibrary.h"
#include "Project48/GameplayMessageLibrary/Core/P48GameplayMessageTags.h"
#include "Project48/GameplayMessageLibrary/Match/P48MatchMessagePayloads.h"
```

프로젝트 내부의 상대 경로를 사용해야 하는 위치에서는 해당 소스 파일 기준으로 경로를 조정합니다.
ex) Character의 경우 Character경로에 있는 Payloads헤더를 호출해 주세요

## 3. 확정된 플레이어 수 보내기

아래 코드는 GameMode에서 플레이어 수를 확정한 뒤 인원 변경 메시지를 보내는 예시입니다.

```cpp
void AP48ExampleGameMode::BroadcastConfirmedPlayerCount()
{
	// 세션/매치 정책에 따라 최종 확정된 인원이어야 합니다.
	const int32 ConfirmedPlayerCount = GetNumPlayers();
	if (ConfirmedPlayerCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Confirmed player count was not sent: no players."));
		return;
	}

	FP48MatchPlayerCountMessage Message;
	Message.PlayerCount = ConfirmedPlayerCount;
	const bool bBroadcast = UP48GameplayMessageLibrary::Broadcast(
		this,
		P48GameplayTags::Match::PlayerCountChanged,
		Message);

	if (!bBroadcast)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to broadcast the confirmed player count."));
	}
}
```

중요한 점은 접속 중인 인원을 임시로 조회하는 시점이 아니라, 게임 시작 정책상 인원이 확정된 시점에 한 번 보내는 것입니다. `PlayerCount`가 0 이하인 메시지는 Maps 수신부에서 거부합니다.

## 4. Payload가 있는 메시지 수신하기

Maps의 실제 수신 구현은 `UP48PCGSeedWorldSubsystem`에 있습니다.

### Listener Handle 보관

```cpp
FGameplayMessageListenerHandle PlayerCountHandle;
```

Handle을 멤버로 보관해야 나중에 정확한 Listener를 해제할 수 있습니다.

### Listener 등록

```cpp
void UP48PCGSeedWorldSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);

	PlayerCountHandle = UP48GameplayMessageLibrary::Listen(
		this,
		P48GameplayTags::Match::PlayerCountChanged,
		&ThisClass::HandlePlayerCountChanged);
}
```

`Listen`의 Payload 타입은 Handler의 두 번째 인자를 통해 결정됩니다. 송신할 때 사용한 Payload 타입과 정확히 같아야 합니다.

### 메시지 처리

```cpp
void UP48PCGSeedWorldSubsystem::HandlePlayerCountChanged(
	FGameplayTag Channel,
	const FP48MatchPlayerCountMessage& Message)
{
	SetRequiredPlayerCount(Message.PlayerCount);
}
```

Handler에서는 전달받은 값을 다시 GameMode에서 조회하지 않습니다. `PlayerCount`는 대기 중인 PlayerStart 생성과 준비 판정에만 사용하며 섬·다리의 Seed나 Revision을 변경하지 않습니다.

### Listener 해제

```cpp
void UP48PCGSeedWorldSubsystem::Deinitialize()
{
	UP48GameplayMessageLibrary::StopListening(PlayerCountHandle);
	Super::Deinitialize();
}
```

WorldSubsystem은 `Deinitialize`, Actor나 ActorComponent는 일반적으로 `EndPlay`에서 해제합니다. `StopListening`은 등록할 때 받은 Handle을 무효화하므로 같은 Handle을 다시 해제할 필요는 없습니다.

## 5. Payload 없이 사건만 알리기

값을 전달할 필요 없이 “어떤 사건이 발생했다”는 사실만 알릴 때는 `BroadcastEvent`와 `ListenEvent`를 사용합니다.

아래의 `EventChannel`은 Payload 없는 이벤트 용도로 선언된 Gameplay Tag라고 가정합니다.

### 이벤트 발생

```cpp
void BroadcastSimpleEvent(const UObject* WorldContextObject, const FGameplayTag EventChannel)
{
	UP48GameplayMessageLibrary::BroadcastEvent(WorldContextObject, EventChannel);
}
```

### 이벤트 수신과 해제

```cpp
FGameplayMessageListenerHandle EventHandle;

void UP48ExampleSubsystem::RegisterEventListener(const FGameplayTag EventChannel)
{
	EventHandle = UP48GameplayMessageLibrary::ListenEvent(
		this,
		EventChannel,
		&ThisClass::HandleEvent);
}

void UP48ExampleSubsystem::HandleEvent(FGameplayTag Channel)
{
	UE_LOG(LogTemp, Display, TEXT("Event received: %s"), *Channel.ToString());
}

void UP48ExampleSubsystem::Deinitialize()
{
	UP48GameplayMessageLibrary::StopListening(EventHandle);
	Super::Deinitialize();
}
```

Payload가 있는 채널에 `BroadcastEvent`를 섞어 쓰지 않습니다. 한 채널은 하나의 Payload 규약으로 사용하는 것이 안전합니다.

## 6. 여러 메시지를 수신하는 경우

메시지마다 Handle을 따로 보관하고 모두 해제합니다.

```cpp
FGameplayMessageListenerHandle GenerationRequestHandle;
FGameplayMessageListenerHandle OtherEventHandle;

void UP48ExampleSubsystem::Deinitialize()
{
	UP48GameplayMessageLibrary::StopListening(GenerationRequestHandle);
	UP48GameplayMessageLibrary::StopListening(OtherEventHandle);
	Super::Deinitialize();
}
```

## 7. 사용 시 확인할 사항

1. Broadcast와 Listen이 같은 Gameplay Tag를 사용하는지 확인합니다.
2. 양쪽의 Payload 타입이 정확히 같은지 확인합니다.
3. 올바른 `WorldContextObject`를 전달했는지 확인합니다.
4. 서버 권한 데이터는 서버에서 Broadcast합니다.
5. 수신 객체의 수명이 끝날 때 `StopListening`을 호출합니다.
6. 플레이어 수는 PCG 맵 생성 입력이 아니라 PlayerStart 준비 조건으로만 사용합니다.

## 8. 관련 코드 위치

- 수신 및 해제: `Maps/PCG/Common/P48PCGSeedWorldSubsystem.cpp`
- Listener Handle: `Maps/PCG/Common/P48PCGSeedWorldSubsystem.h`
- 채널 선언: `GameplayMessageLibrary/Core/P48GameplayMessageTags.h`
- Payload 선언: `GameplayMessageLibrary/Match/P48MatchMessagePayloads.h`
- Broadcast/Listen/StopListening 구현: `GameplayMessageLibrary/Core/P48GameplayMessageLibrary.h`
