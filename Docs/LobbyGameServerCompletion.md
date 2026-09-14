# Lobby / game-server completion contract

The lobby and each game server run in separate processes. A call in the game
server cannot directly invoke `AP48LobbyGameMode` in the lobby process.

## Lobby-to-game travel values

Initial participants receive these client travel URL options:

- `LobbyRoomId`: lobby room number.
- `LobbyMatchId`: GUID for this assignment of the game server.
- `LobbyMemberId`: this player's existing lobby-return ticket ID.
- `ExpectedPlayers`: initial participant count, identical on every initial
  participant's URL. Late spectator travel omits `ExpectedPlayers`.

The GUIDs are correlation IDs, not authentication credentials. The game server
must read the URL options at player login and validate that the initial clients
agree on the expected count. The current game mode only reads `ExpectedPlayers`
from its initial map URL, so the game-server owner must add per-match update
logic before the client URL option affects a reused server.

## Game-server-to-lobby notifications still to be connected

The game-server owner will send two authenticated notifications, both containing
`LobbyRoomId` and `LobbyMatchId`:

1. **Match ended:** after the match has been ended, call the lobby-side
   `AP48LobbyGameMode::ReportGameSessionEnded(RoomId, MatchId)`. Send this even
   if every player disconnected and no client can return. The lobby starts its
   return grace period upon this notification and counts actual lobby returnees.
2. **Server reset complete:** after the old match state has been reset and the
   server can accept a new assignment, call
   `AP48LobbyGameMode::ReportGameServerReady(RoomId, MatchId)`.

For the agreed disconnect policy, the game server keeps playing with at least
two connected match participants. At one or zero, it ends the match through its
normal end sequence, returns any surviving player to the lobby, resets the game
world/state (including `MatchPhase` and `bLobbyReturnRequested`), and sends the
notifications above. A normal match end must send the same two notifications.

The server-to-server transport, authentication, retry and acknowledgement are
not implemented here. Do not expose either lobby method as a client RPC or
trust a client-visible GUID as proof that a report came from the game server.

## Lobby behavior already implemented

On the match-ended notification, the lobby starts the return grace period even
if no player returns. It keeps valid return tickets during that period. Once
all expected players return or the grace period expires, it discards missing
travel records and counts players actually connected to that lobby room. With
zero or one player, it closes the room and sends any survivor to the room list.
With two or more, the room remains available for another match.

The assigned game-server address stays reserved until its reset-complete
notification arrives. If the room closes first, the reservation is retained
separately. Closing a room never stops the lobby service process.
