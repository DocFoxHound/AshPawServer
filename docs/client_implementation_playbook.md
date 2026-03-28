# Client Implementation Playbook

This guide is for the agent or engineer building the client that connects to AshPaw Server. It translates the wire contract into concrete client behavior.

Read this together with:
- [client_server_contract.md](/home/martinb/Documents/AshPawServer/docs/client_server_contract.md)
- [protocol.md](/home/martinb/Documents/AshPawServer/docs/protocol.md)
- [configuration.md](/home/martinb/Documents/AshPawServer/docs/configuration.md)

## First Build Target

The client’s first milestone should be:
1. connect to the server
2. complete handshake
3. spawn the local player
4. show remote players
5. send movement input
6. reconcile to authoritative snapshots
7. render doors, seats, containers, and signs from server state
8. support chat and identity updates

Do not start with client-side authority. Start with a server-trusting client that is boring but correct.

## Recommended Client Systems

Build these client modules explicitly:

### 1. Connection State Machine

Suggested states:
- `disconnected`
- `connecting`
- `waiting_for_server_hello`
- `waiting_for_join_accepted`
- `active`
- `disconnecting`

Suggested transitions:
- ENet connect event -> `waiting_for_server_hello`
- `server_hello` -> still waiting for `join_accepted`
- `join_accepted` -> `active`
- disconnect or `join_rejected` -> `disconnected`

### 2. Authoritative World Store

Keep separate stores keyed by authoritative IDs:
- `entities[entity_id]`
- `identities[entity_id]`
- `interactables[target_id]`
- `local_controlled_entity_id`

Do not key world objects by scene-node IDs, render object IDs, or client-generated UUIDs.

### 3. Network Decoder

Handle packets by opcode and feed them into the authoritative world store.

Recommended rule:
- transport decode should not directly mutate rendering objects
- decode into authoritative game state first
- rendering should consume authoritative state second

### 4. Reconciliation Layer

If the client predicts local movement:
- prediction is presentation only
- snapshots correct the truth
- the local player should reconcile to authoritative snapshots rather than ignoring them

If prediction is not implemented yet:
- that is acceptable
- just render authoritative state cleanly

## Packet Handling Strategy

### `join_accepted`

On `join_accepted`:
- store `session_id`
- store `local_controlled_entity_id`
- create or refresh the local player entity record at `spawn_x`, `spawn_y`

Do not assume this is the only spawn packet you will receive for the local player.

### `player_spawn`

Treat as upsert:
- if entity does not exist, create it
- if entity already exists, refresh its authoritative position

This matters because the current server implementation may send duplicate spawn info during the initial join flow.

### `player_despawn`

Treat as delete:
- remove entity from authoritative entity store
- remove local presentation objects derived from that entity
- remove identity mapping only if your client architecture requires identity cleanup on despawn

### `transform_snapshot`

Treat as delta updates, not full replacement.

Correct behavior:
- update only the listed entities
- do not despawn entities missing from the snapshot
- keep prior known state for unmentioned in-range entities

Incorrect behavior:
- replacing the entire world entity set with snapshot contents

### `identity_update`

Treat as authoritative label binding:
- store display name by `entity_id`
- update nameplates/chat speaker labels/UI displays from this mapping

### `object_state_update`

Use this to drive interactable visuals and interaction affordances.

Interpretation:
- `target_id`: authoritative object key
- `is_open`: door/container open state
- `occupant_entity_id`: seat occupant entity or `0` for empty

### `interaction_result`

Use this as the authoritative answer for:
- success/failure
- server-side sign text
- blocked/out-of-range feedback

Do not present a requested interaction as successful before this packet arrives.

### `chat_broadcast`

Render this as the authoritative delivered message, not as a local optimistic echo that bypasses server state.

## Input Strategy

### Movement

Send `movement_input` regularly while active:
- channel `1` preferred
- directional values only
- expected range is `[-1, 1]`

Recommended approach:
- send when input changes
- optionally resend while held at a modest cadence
- send zeroes when input stops if your client architecture depends on explicit stop messages

### Interaction

Send `interaction_request(target_id)` only when:
- the local client has a selected or targeted interactable
- the client is in `active` state

The server will validate range and target existence.

### Chat

Send `chat_send(message)` on channel `0`.

Mirror server validation client-side:
- keep messages at or below configured max length
- do not send empty messages

## Rendering Strategy

The renderer should be downstream of authoritative state.

Suggested split:
- networking decodes packets
- authoritative world store applies them
- presentation layer interpolates from authoritative state

For transforms:
- remote entities can interpolate between last rendered and latest authoritative position
- local entity can either snap or reconcile from predicted state depending on client sophistication

## Common Mistakes To Avoid

Avoid these:
- treating `transform_snapshot` as full world replacement
- using client-generated entity IDs
- assuming the chosen display name is the final authoritative name
- assuming `player_spawn` is unique during join
- assuming lack of snapshot means disconnect
- assuming local interaction success before `interaction_result`
- sending known packets on the wrong ENet channel

## Minimal Acceptance Tests For The Client Agent

The client is not ready until it can do all of these:

1. connect and handshake with protocol version `1`
2. create the local entity from `join_accepted`
3. apply duplicate-safe `player_spawn`
4. apply delta-only `transform_snapshot`
5. show remote identities from `identity_update`
6. move under authoritative correction
7. open/close doors from `object_state_update`
8. sit/stand via seat occupancy updates
9. display sign text from `interaction_result`
10. send and receive chat through `chat_broadcast`
11. reconnect with the same effective display name and restore position

## Practical Handoff To The Client Agent

Tell the client agent to treat these files as the source of truth:
- [client_server_contract.md](/home/martinb/Documents/AshPawServer/docs/client_server_contract.md)
- [client_implementation_playbook.md](/home/martinb/Documents/AshPawServer/docs/client_implementation_playbook.md)
- [protocol.md](/home/martinb/Documents/AshPawServer/docs/protocol.md)

If the client behavior conflicts with those docs, the client should change rather than guessing around the server.
