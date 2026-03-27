# Phase 0-3 Protocol

All packets begin with a one-byte opcode. Multi-byte integers are little-endian. Strings are encoded as a one-byte length followed by raw UTF-8 bytes.

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

### `movement_input`
- `i8 move_x`
- `i8 move_y`

Values are clamped to `[-1, 1]` during decode.

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

### `object_state_update`
- `string target_id`
- `u8 is_open`
- `u32 occupant_entity_id`

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

Phase 5 note:
- transform snapshots are periodic authoritative updates sent on a separate snapshot cadence
- sessions may receive only changed entity transforms rather than a full world dump every send
- replication is filtered by a simple visibility radius rather than broadcasting the whole world to every client
