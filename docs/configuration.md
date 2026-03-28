# Configuration Reference

The server reads configuration from `config/server.toml` and supports CLI overrides for a small subset of values.

## File Fields

### `listen_port`

- Type: unsigned integer
- Default in repo: `7777`
- Meaning: UDP port used by the ENet host

### `tick_rate`

- Type: unsigned integer
- Default in repo: `20`
- Meaning: fixed simulation step rate

### `snapshot_rate`

- Type: unsigned integer
- Default in repo: `10`
- Meaning: authoritative transform snapshot cadence, separate from simulation tick rate

### `max_players`

- Type: unsigned integer
- Default in repo: `16`
- Meaning: max simultaneously tracked peer sessions

### `visibility_radius_units`

- Type: unsigned integer
- Default in repo: `160`
- Meaning: radius used for transform and object-state replication filtering

### `max_packet_size_bytes`

- Type: unsigned integer
- Default in repo: `512`
- Meaning: inbound/outbound packet size hard cap used by network validation and send suppression

### `max_display_name_length`

- Type: unsigned integer
- Default in repo: `24`
- Meaning: post-sanitization maximum display-name length

### `max_chat_message_length`

- Type: unsigned integer
- Default in repo: `120`
- Meaning: authoritative max accepted chat payload length

### `startup_map`

- Type: path string
- Default in repo: `maps/dev_map.json`
- Meaning: map loaded on server startup

### `player_save_dir`

- Type: path string
- Default in repo: `data/players`
- Meaning: directory used for player persistence files and `.bak.json` backups

### `log_level`

- Type: string
- Accepted values: `trace`, `debug`, `info`, `warn`, `error`
- Meaning: startup logging verbosity

## CLI Overrides

Supported CLI flags:
- `--config <path>`
- `--port <value>`
- `--log-level <trace|debug|info|warn|error>`

Notes:
- `--port` overrides `listen_port`
- `--log-level` overrides `log_level`
- other fields are file-driven only at the moment

## Operational Guidance

- Keep `player_save_dir` on persistent storage if reconnect persistence matters.
- Keep `max_packet_size_bytes` comfortably above the largest expected reliable payloads.
- Avoid setting `snapshot_rate` higher than the client can process meaningfully; simulation and replication are intentionally separate.
- Choose `visibility_radius_units` based on gameplay relevance, not full-map convenience, because it directly affects replication volume.
