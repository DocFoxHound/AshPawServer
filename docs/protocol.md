# AshPaw Server Protocol

All packets begin with a one-byte opcode. Multi-byte integers are little-endian. Strings are encoded as a one-byte length followed by raw UTF-8 bytes.

Protocol version: `1`

## Channels

- Channel `0`: reliable traffic
- Channel `1`: movement input traffic

Expected client packet routing:
- `client_hello` on channel `0`
- `interaction_request` on channel `0`
- `chat_send` on channel `0`
- `movement_input` on channel `1` preferred, channel `0` accepted

If a client sends a known opcode on the wrong channel, the server disconnects that session as malformed traffic.

## Limits

- `max_packet_size_bytes`: configured in `config/server.toml`, default `512`
- `max_display_name_length`: configured in `config/server.toml`, default `24`
- `max_chat_message_length`: configured in `config/server.toml`, default `120`
- String fields use a one-byte length prefix, so protocol-level maximum encoded string length is `255`

Display-name sanitation rules on connect:
- the server keeps only ASCII letters, digits, spaces, `-`, and `_`
- the server trims leading/trailing spaces
- the server truncates to `max_display_name_length`
- if nothing remains, the server uses `player`

## Opcodes

- `1`: `client_hello`
- `2`: `server_hello`
- `3`: `join_accepted`
- `4`: `join_rejected`
- `5`: `movement_input`
- `6`: `player_spawn`
- `7`: `player_despawn`
- `8`: `transform_snapshot`
- `9`: `interaction_request`
- `10`: `interaction_result`
- `11`: `object_state_update`
- `12`: `chat_send`
- `13`: `chat_broadcast`
- `14`: `identity_update`

## Payloads

### `client_hello`
- `u16 protocol_version`
- `string display_name`

### `server_hello`
- `u16 protocol_version`
- `u16 tick_rate`

### `join_accepted`
- `u32 session_id`
- `u32 entity_id`
- `f32 spawn_x`
- `f32 spawn_y`

### `join_rejected`
- `u8 reason_code`
- `string message`

Reject reasons:
- `1`: invalid protocol
- `2`: server full
- `3`: malformed packet

### `movement_input`
- `i8 move_x`
- `i8 move_y`

Server behavior:
- values are clamped to `[-1, 1]` during decode
- client positions are never trusted

### `player_spawn`
- `u32 entity_id`
- `f32 x`
- `f32 y`

### `player_despawn`
- `u32 entity_id`

### `interaction_request`
- `string target_id`

### `interaction_result`
- `u8 status`
- `string target_id`
- `string message`

Interaction statuses:
- `0`: success
- `1`: not_found
- `2`: out_of_range
- `3`: blocked
- `4`: invalid_target

### `object_state_update`
- `string target_id`
- `u8 is_open`
- `u32 occupant_entity_id`

Notes:
- `occupant_entity_id == 0` means no occupant
- currently used for doors, seats, and containers

### `chat_send`
- `string message`

### `chat_broadcast`
- `u32 entity_id`
- `string display_name`
- `string message`

### `identity_update`
- `u32 entity_id`
- `string display_name`

### `transform_snapshot`
- `u16 entity_count`
- repeated:
  - `u32 entity_id`
  - `f32 x`
  - `f32 y`

## Session Flow

1. Client connects over ENet.
2. Client sends `client_hello`.
3. Server validates protocol and sanitized name.
4. Server sends `server_hello`.
5. Server restores or spawns the authoritative player entity.
6. Server sends `join_accepted`.
7. Server sends current `player_spawn` packets for visible entities and current `identity_update` packets.
8. Server sends initial `object_state_update` packets for nearby interactables.
9. Server continues sending `transform_snapshot` packets on the snapshot cadence.

## Replication Semantics

- Snapshots are not a full world dump every send.
- The server uses visibility filtering with a configured radius.
- The server tracks per-session last-replicated transforms and object states.
- A client may receive no snapshot packet for a send interval if nothing changed in range.
- Reliable events like spawn, despawn, identity, interaction results, and object state updates can arrive separately from snapshots.

## Authoritative Rules

- The client may request movement, chat, and interactions.
- The server decides positions, interaction outcomes, object state, identity, and chat broadcast.
- On disconnect, the server despawns the player entity and persists player state.
