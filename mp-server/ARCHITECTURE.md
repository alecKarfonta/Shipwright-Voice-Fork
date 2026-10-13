# Hyrule Reborn multiplayer architecture

This codebase is an eight-player co-op service evolving toward an MMO. It is
not yet a production MMO. The native client owns combat, gear, currency and
character saves. The relay owns room membership, room rules, social identity
within a connection, shared quest coordination and packet routing.

## Current boundaries

- TCP and binary WebSocket carry the same null-delimited JSON protocol.
- One game process owns its game state. Network callbacks only advance a
  connection epoch or enqueue messages. Game-thread processing updates hooks,
  actors, saves and UI. Old-connection messages cannot enter a new session.
- One relay process owns its rooms. Do not scale its deployment beyond one
  replica until room placement and durable ownership are implemented.
- Each relay connection has one writer, 128 queued frames and a 2 MiB budget;
  all writers share a 64 MiB pending-byte budget. A slow consumer disconnects
  instead of stalling a room or silently losing reliable events.
- The client bounds each incoming/outgoing queue to 1,024 messages / 4 MiB of
  serialized payload. Overflow stops the transport; reconnect starts fresh.
- Clients cannot relay server-only packets or unknown message classes. Peer
  identity comes from the authenticated transport session (currently an
  anonymous connection, not an account). Invalid roster updates are dropped.
- Team progress messages stay inside the sender's team. Damage requires the
  room's PvP permission, compatible version/seed and the same scene/room.
  Teleport responses require a permitted request within ten seconds and can
  be consumed once. Combat hits themselves remain client reported.

## Fellowship protocol, version 1

After admission, `SERVER_CAPABILITIES` advertises `socialProtocol: 1`.
Clients send `SOCIAL` with `version: 1` and `op: "hello"` to subscribe. Older
clients continue using co-op. Updated clients on older relays show the roster
and an explanation of unavailable chat/ready checks.

| Operation | Request | Server behavior |
| --- | --- | --- |
| `hello` | no additional fields | Snapshot: room ID, party leader, visible history and ready check. |
| `chat` | `channel` (`party` or `room`), `text`, `requestId` | Assigns sender/name/ID/time; echoes acceptance. Four-message burst, then one every two seconds; 480 UTF-8 bytes after sanitizing controls/format characters. |
| `ready_check` | no additional fields | Party leader only; snapshots the current party for 60 seconds. |
| `ready` | `checkId`, Boolean `ready` | Current participant only; rejects expired checks and late joiners. |
| `promote` | `targetClientId` | Room owner only; atomically changes room ownership and republishes rules. |

Party scope is team + seed + client version. The room owner leads its party;
other parties use their lowest active connection ID. Departures or scope changes
mark the member departed without shrinking the ready-check roster. Snapshots
carry a relative remaining time, so client clock skew does not affect the timer.
The server retains 100 messages per room; party history is filtered before
sending. It is ephemeral and disappears when the room empties or relay restarts.

The journal's Fellowship page provides party selection, private party/room chat,
ready checks, travel actions and deliberate leadership transfer. Its compact
layout separates Party and Chat while retaining the same controls. Messages are
confirmed by the server before the input clears; an unconfirmed draft survives a
disconnect. The original save screen and journal continue to own character and
pause navigation.

## Operation and verification

Run the service locally without external exposure:

```sh
go run . -bind 127.0.0.1 -port 43383 -http 127.0.0.1:8080
```

`/healthz` reports process liveness. `/readyz` fails during drain. `/metrics`
exports aggregate connections, rooms, queued bytes and slow-consumer disconnects
in Prometheus text format. SIGINT/SIGTERM close listeners and all tracked TCP and
hijacked WebSocket sessions with an eight-second shutdown bound. There is no
persistent state to flush yet; the normal client save remains essential.

```sh
go test -race ./...
go vet ./...
cmake -S ../tests/public-relay -B /tmp/z64-relay-tests
cmake --build /tmp/z64-relay-tests
ctest --test-dir /tmp/z64-relay-tests --output-on-failure
```

Go tests start their own ephemeral-port listener; unavailable services fail tests
rather than silently skipping them. Coverage includes social scope isolation,
server-assigned chat identity, rate/history bounds, readiness authority and
expiry, disconnects, leadership transfer, untrusted packet classes, requested
teleports, and slow-reader ordering/memory release. Portable C++ tests cover
Fellowship history, expiry and atomic rejection of malformed snapshots.

`../tests/public-relay/fellowship_fixture.py` provides explicitly labeled local
QA peers for visual testing. It requires the native QA character to join first;
it never connects to the public relay or accesses normal character saves.

## Remaining production gates

1. **Accounts and durable characters:** authenticated identities, account
   recovery, secure session storage, character ownership, schema migrations,
   server-side snapshots, backups and restore drills. Define a separate verified
   realm for new characters; local imported saves must not silently gain trusted
   economic status.
2. **World and encounter authority:** fixed simulation ticks, authoritative
   enemy health/spawn/kill records, reconciled movement, instanced encounters,
   loot ownership and idempotent reward claims. Today a client can still invent
   combat and some quest events; do not enable a persistent shared economy yet.
3. **Economy:** transactional inventory/currency, unique item IDs, replay-safe
   commands and audit trails before enabling trading, mail, auctions or guild
   banks. Preserve all existing ARPG item/crafting/skill semantics during this
   migration and reconcile full-inventory rewards safely.
4. **Realm services:** admission queues, room/zone placement, reconnect leases,
   crash recovery, interest management and horizontal scaling. The room cap is
   a co-op constraint, not evidence of MMO concurrency capacity.
5. **Social safety and operations:** durable friends/guilds/invitations,
   account-based blocks, moderation/report workflows, access control for operator
   actions, incident runbooks and privacy/retention decisions.
6. **Client polish and accessibility:** consistent native menu components,
   controller and keyboard focus, readable scaling/localization, screen-safe
   layouts, visible connection/recovery state, actionable errors and audio cues.
   Verify all pages at 640x480 and modern widescreen sizes on actual hardware.
7. **Release evidence:** Windows and macOS builds and native controller tests;
   multi-hour multiplayer/encounter soak; adversarial protocol/fuzz testing;
   measured target concurrency/latency/memory budgets; interrupted-save and
   backup recovery tests. Publish binaries only after these gates are recorded.

Track completed work and evidence here without labeling unverified gates as done.
