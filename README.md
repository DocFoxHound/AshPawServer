# AshPaw Server

AshPaw Server is the authoritative multiplayer backend for a top-down 2D animal roleplay RPG. This repo owns world simulation, player sessions, networking authority, validation, and state replication. Rendering and client presentation stay out of scope.

The current milestone is Phase 0-3:
- bootstrap a cross-platform headless C++ server
- run a fixed simulation tick
- accept ENet client connections and handshake them into sessions
- spawn players and move them authoritatively
- broadcast coherent snapshots so two clients can observe shared movement

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
- `startup_map`: map file loaded on startup
- `player_save_dir`: file-backed player save directory for prototype persistence

## Repo Layout

- `src/`, `include/`: server runtime
- `tests/`: unit and integration-style tests
- `config/`: local server config
- `maps/`: starter map data
- `docs/`: protocol and content contracts

## First Success Condition

The first real proof that this repo is working is when two clients can connect, create sessions, spawn into the world, move under server authority, and receive stable shared snapshots.
