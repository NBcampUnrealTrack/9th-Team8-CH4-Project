#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Project48/Lobby/P48LobbyGameMode.h"
#include "Project48/Lobby/P48LobbyGameState.h"
#include "Project48/Lobby/P48LobbyTravelSubsystem.h"
#include "Project48/Character/P48PlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP48LobbyRoomRetentionTest,
	"P48.Lobby.RoomRetention", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP48LobbyRoomRetentionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	AP48PlayerState* Host = World->SpawnActor<AP48PlayerState>();
	AP48PlayerState* Guest = World->SpawnActor<AP48PlayerState>();
	AP48LobbyGameState* GameState = World->SpawnActor<AP48LobbyGameState>();
	if (!Host || !Guest || !GameState)
	{
		AddError(TEXT("Could not spawn lobby test actors"));
		World->DestroyWorld(false);
		return false;
	}
	Host->SetPlayerName(TEXT("Host"));
	Guest->SetPlayerName(TEXT("Guest"));

	FP48LobbyRoomState WaitingRoom;
	WaitingRoom.Participants = { Host, Guest };
	WaitingRoom.HostPlayerState = Host;
	WaitingRoom.RemoveParticipant(Host, true);
	TestEqual(TEXT("Normal disconnect removes membership"), WaitingRoom.GetMemberCount(), 1);
	TestTrue(TEXT("Normal host departure clears the live host"), WaitingRoom.HostPlayerState == nullptr);
	WaitingRoom.RemoveParticipant(Guest, false);
	TestTrue(TEXT("An ordinary empty room can be deleted"), WaitingRoom.IsEmpty());

	FP48LobbyRoomState TravelingRoom;
	TravelingRoom.RoomId = 1;
	TravelingRoom.RoomTitle = TEXT("Retention Test Room");
	TravelingRoom.Password = TEXT("1234");
	TravelingRoom.Participants = { Host, Guest };
	TravelingRoom.HostPlayerState = Host;
	TravelingRoom.RecordGameHandoff(TEXT("127.0.0.1:17777"));
	const FGuid HostId = TravelingRoom.TravelMembers[0].MemberId;
	TestTrue(TEXT("Member ID is generated"), HostId.IsValid());
	TestTrue(TEXT("Member IDs are distinct"), HostId != TravelingRoom.TravelMembers[1].MemberId);
	TestTrue(TEXT("Recorded members are marked at the game server"),
		TravelingRoom.TravelMembers[0].bAtGameServer
		&& TravelingRoom.TravelMembers[1].bAtGameServer);
	TestEqual(TEXT("No double count before disconnect"), TravelingRoom.GetMemberCount(), 2);
	TravelingRoom.RecordGameHandoff(TEXT("127.0.0.1:17777"));
	TestEqual(TEXT("Recording an existing dispatch is idempotent"), TravelingRoom.TravelMembers.Num(), 2);
	TravelingRoom.RemoveParticipant(Host, true);
	TestEqual(TEXT("Host remains a member while away"), TravelingRoom.GetMemberCount(), 2);
	TravelingRoom.RemoveParticipant(Guest, true);
	TestTrue(TEXT("No strong lobby participant references remain"), TravelingRoom.Participants.IsEmpty());
	TestFalse(TEXT("Traveling room is not deleted"), TravelingRoom.IsEmpty());
	TestEqual(TEXT("Stable count after everyone leaves the lobby"), TravelingRoom.GetMemberCount(), 2);
	TestEqual(TEXT("Destination is retained"), TravelingRoom.AssignedGameServerAddress, FString(TEXT("127.0.0.1:17777")));
	TestEqual(TEXT("Password is retained"), TravelingRoom.Password, FString(TEXT("1234")));
	TestEqual(TEXT("Room title is retained"), TravelingRoom.RoomTitle,
		FString(TEXT("Retention Test Room")));
	TestTrue(TEXT("Original host record is retained"), TravelingRoom.TravelMembers[0].bIsHost);
	TestTrue(TEXT("Record ID survives disconnect"), TravelingRoom.TravelMembers[0].MemberId == HostId);
	TestFalse(TEXT("Detached actor is not needed"), TravelingRoom.TravelMembers[0].LobbyPlayerState.IsValid());

	AP48PlayerState* LateGuest = World->SpawnActor<AP48PlayerState>();
	if (!TestNotNull(TEXT("Late guest"), LateGuest))
	{
		World->DestroyWorld(false);
		return false;
	}
	TravelingRoom.Participants.Add(LateGuest);
	TestEqual(TEXT("Late guest counts alongside the two retained members"), TravelingRoom.GetMemberCount(), 3);
	TestEqual(TEXT("Late room entry does not dispatch the guest"), TravelingRoom.TravelMembers.Num(), 2);
	TestTrue(TEXT("Original traveling host is preserved"), TravelingRoom.TravelMembers[0].bIsHost);
	TestTrue(TEXT("Late guest is not made the live host"), TravelingRoom.HostPlayerState == nullptr);
	TestTrue(TEXT("Ready late guest can be recorded for handoff"), TravelingRoom.RecordMemberHandoff(LateGuest));
	TestFalse(TEXT("Late guest cannot be recorded twice"), TravelingRoom.RecordMemberHandoff(LateGuest));
	TestEqual(TEXT("Recording handoff does not double-count the late guest"), TravelingRoom.GetMemberCount(), 3);
	TestEqual(TEXT("Only the late guest was added to travel records"), TravelingRoom.TravelMembers.Num(), 3);
	TravelingRoom.RemoveParticipant(LateGuest, true);
	TestEqual(TEXT("Late guest remains reserved after travel disconnect"), TravelingRoom.GetMemberCount(), 3);
	TestFalse(TEXT("The game room survives the late guest traveling"), TravelingRoom.IsEmpty());

	TArray<FP48LobbyPlayerEntry> Entries;
	for (const FP48LobbyTravelMember& Member : TravelingRoom.TravelMembers)
	{
		FP48LobbyPlayerEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.RoomId = TravelingRoom.RoomId;
		Entry.PlayerName = Member.PlayerName;
		Entry.bIsHost = Member.bIsHost;
		Entry.bTravelRequested = true;
	}
	FP48LobbyRoomInfo Info;
	Info.RoomId = 1;
	Info.RoomTitle = TravelingRoom.RoomTitle;
	Info.CurrentPlayers = 2;
	Info.bHasGameServer = true;
	Info.bIsReturningToLobby = true;
	GameState->RebuildLobbyState(Entries, { Info });
	TestFalse(TEXT("Null actor cannot match a retained host"), GameState->CanPlayerStartGame(nullptr));
	TestFalse(TEXT("Archived room cannot restart"), GameState->CanHostStartGame());
	TestTrue(TEXT("Roster survives without actors"), GameState->GetLobbyPlayerListTextForRoom(1).ToString().Contains(TEXT("Host / Travel Requested")));
	TestEqual(TEXT("Room remains discoverable"), GameState->GetLobbyRooms().Num(), 1);
	TestEqual(TEXT("Room title is exposed in room info"),
		GameState->GetLobbyRooms()[0].RoomTitle, FString(TEXT("Retention Test Room")));
	TestTrue(TEXT("Returning room state is exposed to clients"),
		GameState->IsRoomReturningToLobby(1));
	TestTrue(TEXT("Returning room has a visible status message"),
		GameState->GetRoomStatusText(1).ToString().Contains(TEXT("return")));
	Info.bIsReturningToLobby = false;
	GameState->RebuildLobbyState(Entries, { Info });
	TestEqual(TEXT("Active game room has an in-progress status"),
		GameState->GetRoomStatusText(1).ToString(), FString(TEXT("Game in progress")));

	AP48PlayerState* ReturnedHost = World->SpawnActor<AP48PlayerState>();
	AP48PlayerState* ReturnedGuest = World->SpawnActor<AP48PlayerState>();
	AP48PlayerState* ReturnedLateGuest = World->SpawnActor<AP48PlayerState>();
	if (!ReturnedHost || !ReturnedGuest || !ReturnedLateGuest)
	{
		AddError(TEXT("Could not spawn returning lobby actors"));
		World->DestroyWorld(false);
		return false;
	}
	const FGuid GuestId = TravelingRoom.TravelMembers[1].MemberId;
	const FGuid LateGuestId = TravelingRoom.TravelMembers[2].MemberId;
	TestTrue(TEXT("Host return ticket restores room membership"),
		TravelingRoom.RestoreTravelMember(HostId, ReturnedHost));
	TestFalse(TEXT("A consumed return ticket cannot be reused"),
		TravelingRoom.RestoreTravelMember(HostId, World->SpawnActor<AP48PlayerState>()));
	TestTrue(TEXT("Returned host ownership is restored"),
		TravelingRoom.HostPlayerState == ReturnedHost);
	TestFalse(TEXT("Room is not reset while game members remain away"),
		TravelingRoom.AreAllTravelMembersBack());
	TestTrue(TEXT("Guest return ticket restores room membership"),
		TravelingRoom.RestoreTravelMember(GuestId, ReturnedGuest));
	TestTrue(TEXT("Late guest return ticket restores room membership"),
		TravelingRoom.RestoreTravelMember(LateGuestId, ReturnedLateGuest));
	TestTrue(TEXT("All game members can be detected as returned"),
		TravelingRoom.AreAllTravelMembersBack());
	TestEqual(TEXT("Returning actors do not increase the stable member count"),
		TravelingRoom.GetMemberCount(), 3);

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UP48LobbyTravelSubsystem* TravelSubsystem =
		NewObject<UP48LobbyTravelSubsystem>(TestGameInstance);
	TestTrue(TEXT("Valid return context is stored"),
		TravelSubsystem->StoreLobbyReturnContext(TEXT("127.0.0.1:17775"), 1, HostId));
	TestTrue(TEXT("Stored return context is available"),
		TravelSubsystem->HasLobbyReturnContext());
	TravelSubsystem->ClearLobbyReturnContext();
	TestFalse(TEXT("Cleared return context is unavailable"),
		TravelSubsystem->HasLobbyReturnContext());
	TestFalse(TEXT("Malformed return address is rejected"),
		TravelSubsystem->StoreLobbyReturnContext(TEXT("127.0.0.1:17775?bad=1"), 1, HostId));

	FP48LobbyRoomState ExplicitLeaveRoom;
	ExplicitLeaveRoom.Participants = { Guest };
	ExplicitLeaveRoom.RecordGameHandoff(TEXT("127.0.0.1:17777"));
	ExplicitLeaveRoom.RemoveParticipant(Guest, false);
	TestTrue(TEXT("Explicit leave removes the pending record"), ExplicitLeaveRoom.IsEmpty());
	World->DestroyWorld(false);
	return true;
}

#endif
