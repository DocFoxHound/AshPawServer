# AshPaw Server

AshPaw Server is the authoritative multiplayer backend for a top-down 2D animal roleplay RPG. This repo owns world simulation, player sessions, networking authority, validation, and state replication. Rendering and client presentation stay out of scope.

The current milestone is Phase 0-10 prototype coverage:
- bootstrap a cross-platform headless C++ server
- run a fixed simulation tick with snapshot replication
- accept ENet clients and keep movement/interactions/chat authoritative
- persist prototype player identity and location across reconnects
- expose admin/debug commands for development workflows

## Dependencies

- CMake 3.25+
- a C++20 compiler
- vcpkg in manifest mode

## Build

### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build
ctest --test-dir build --output-on-failure
```

### Windows PowerShell

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -C Debug
```

## Run

```bash
./build/ashpaw_server --config config/server.toml
```

Optional flags:
- `--config <path>`
- `--port <value>`
- `--log-level <trace|debug|info|warn|error>`

Core config fields:
- `listen_port`: ENet listen port
- `tick_rate`: simulation cadence
- `snapshot_rate`: transform replication cadence, separate from simulation tick
- `visibility_radius_units`: simple area-of-interest radius for transform and object replication
- `max_packet_size_bytes`: hard cap for inbound and outbound packet size
- `max_display_name_length`: sanitized authoritative display-name limit
- `max_chat_message_length`: authoritative chat payload limit
- `startup_map`: map file loaded on startup
- `player_save_dir`: file-backed player save directory for prototype persistence

Admin console commands:
- `help`
- `players`
- `kick <name|entity_id>`
- `teleport <name|entity_id> <x> <y>`
- `inspect <entity_id|interactable_id>`
- `metrics`
- `validate_map`
- `stop`

## Deployment Notes

- Run the server behind a firewall and expose only the configured UDP listen port.
- Keep `player_save_dir` on persistent storage; the server will keep a `.bak.json` copy of the previous player save when overwriting a save file.
- For Linux hosting, run under a process supervisor such as `systemd`, `tmux`, or `screen` so disconnects from the shell do not terminate the server.
- Watch the server logs for map-validation warnings and malformed-packet disconnects during early playtests.

## Basic Hosting

1. Build with the vcpkg toolchain as shown above.
2. Copy `config/server.toml`, `maps/`, and the built server binary to the host machine.
3. Set the desired UDP port in `config/server.toml` and open that port in the host firewall/router.
4. Start the server with `./ashpaw_server --config config/server.toml`.
5. Use the built-in console commands like `players`, `metrics`, and `stop` during live testing.

## Repo Layout

- `src/`, `include/`: server runtime
- `tests/`: unit and integration-style tests
- `config/`: local server config
- `maps/`: starter map data
- `docs/`: protocol and content contracts

## Current Success Condition

Two clients can connect, move, interact, and chat under server authority; reconnect without losing core state; and be inspected or managed through the built-in admin command surface.
