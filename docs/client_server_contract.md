# Client/Server Contract

This document is the handoff contract for the client-side implementation. Build the client to this contract unless the server protocol changes deliberately.

## Non-Negotiable Rules

- The server is authoritative for movement, interactions, object state, identity, chat, and persistence.
- The client must never assume its local predicted position is truth.
- The client must be prepared for snapshots to be sparse, partial, and visibility-filtered.
- The client must treat reliable event packets and snapshot packets as separate streams that both update world state.

## ENet Setup

- Expect two channels.
- Channel `0` is reliable/event traffic.
- Channel `1` is movement input traffic.

Client send rules:
- `client_hello` on channel `0`
- `interaction_request` on channel `0`
- `chat_send` on channel `0`
- `movement_input` on channel `1` preferred

If the client sends known packets on the wrong channel, the server may disconnect it.

## Handshake Sequence

1. Connect peer.
2. Send `client_hello(protocol_version=1, display_name=desired_name)`.
3. Wait for `server_hello`.
4. Wait for `join_accepted`.
5. Accept initial `player_spawn`, `identity_update`, and `object_state_update` packets in any nearby order after join.
6. Start normal gameplay flow.

Important:
- The server may sanitize the display name.
- The client must update UI/state to the authoritative name from `identity_update` and chat broadcasts.
- The authoritative controlled entity is `join_accepted.entity_id`.
- The current implementation may send duplicate `player_spawn` data for the local entity during join; client spawn handling should be idempotent.

## Authoritative Data Model

Treat these as server-owned:
- entity transforms
- interactable open/closed state
- seat occupants
- display names
- chat delivery

Treat these as client requests only:
- movement intent
- interaction requests
- chat-send requests
- requested display name during handshake

## Packet Semantics

### `player_spawn`

Use this to create or refresh a replicated entity record.

### `player_despawn`

Remove the entity immediately.

### `transform_snapshot`

- Contains only changed transforms for entities in visibility range.
- Absence of an entity in one snapshot does not mean despawn.
- Only `player_despawn` means removal.

Recommended client strategy:
- keep a replicated-entity table keyed by `entity_id`
- update positions from authoritative snapshots
- if client prediction exists for the local player, reconcile to snapshot state rather than overwriting blindly with rendering pops

### `identity_update`

- Bind authoritative display names to `entity_id`
- Apply this even if the entity already exists locally

### `object_state_update`

Interpret as:
- `target_id`
- `is_open`
- `occupant_entity_id`

Rules:
- `occupant_entity_id == 0` means no occupant
- seat occupancy is authoritative
- door/container open state is authoritative

### `interaction_result`

This is the authoritative answer to an interaction request.

Current interaction status enum values:
- `0`: success
- `1`: not_found
- `2`: out_of_range
- `3`: blocked
- `4`: invalid_target

Do not assume a requested interaction succeeded until this arrives.

### `chat_broadcast`

This is the authoritative social message event.

Use:
- `entity_id` for speaker identity linkage
- `display_name` as the authoritative rendered label
- `message` as the authoritative chat payload

## Validation and Limits

The client should mirror these rules to reduce rejected traffic:
- protocol version must be `1`
- display-name transport is length-prefixed with one byte
- server sanitizes display names to letters, digits, spaces, `-`, and `_`
- server trims name edges and truncates to configured `max_display_name_length`
- server rejects oversized packets beyond `max_packet_size_bytes`
- server rejects chat messages longer than configured `max_chat_message_length`

Current default config values in this repo:
- `max_packet_size_bytes = 512`
- `max_display_name_length = 24`
- `max_chat_message_length = 120`

## Persistence Expectations

- Player persistence is currently keyed by a normalized form of the authoritative display name.
- Reconnecting with the same effective name should restore identity and position.
- The client should not assume account-backed identity yet.

## Recommended Client Architecture

- Maintain a connection/session state machine:
  - disconnected
  - connecting
  - waiting_for_server_hello
  - waiting_for_join_accepted
  - active
- Maintain entity records keyed by `entity_id`.
- Maintain interactable records keyed by `target_id`.
- Keep identity records keyed by `entity_id`.
- Keep rendering/interpolation separate from network decode.
- Treat snapshots and reliable events as feeds into a shared authoritative world model.

## Current Scope

Implemented server-facing gameplay features:
- movement
- spawn/despawn
- transform snapshots
- doors
- signs
- seats
- containers
- authoritative identity
- authoritative chat
- reconnect persistence
- admin/server tooling on the server side

Not implemented yet on the wire:
- local-area chat channels
- NPC dialogue protocol
- emote routing
- account/auth backend
- map reload protocol

## Client Acceptance Checklist

- Can connect and complete handshake with protocol version `1`
- Uses the authoritative `entity_id` from `join_accepted`
- Sends movement on channel `1`
- Applies `player_spawn` and `player_despawn` correctly
- Reconciles `transform_snapshot` instead of treating snapshots as full-state replacement
- Applies `identity_update` and `chat_broadcast` as authoritative
- Applies `object_state_update` for doors, seats, and containers
- Handles reconnect restore without local assumptions about spawn position
