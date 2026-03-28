# Release Readiness Checklist

## Server

- `cmake --build build-vcpkg` passes on the target branch
- `ctest --test-dir build-vcpkg --output-on-failure` passes cleanly
- `config/server.toml` is present and reviewed for the deployment environment
- startup map loads without validation warnings that would block playtests
- `player_save_dir` points to persistent storage
- UDP `listen_port` is open in the firewall/router

## Protocol

- client and server both use protocol version `1`
- client routes packets to the correct ENet channels
- client respects server authority for position, interaction outcomes, identity, and object state
- client handles missing snapshot intervals without assuming disconnect
- client treats `occupant_entity_id == 0` as empty seat/container occupant state

## Playtest

- two clients can connect simultaneously
- movement remains authoritative under repeated connect/disconnect cycles
- doors, signs, seats, and containers replicate correctly
- chat works and identity labels stay consistent
- reconnect restores player identity and location
- malformed or wrong-channel traffic disconnects cleanly without crashing the server

## Ops

- logs are captured by the deployment environment
- the server is launched under a supervisor like `systemd`, `tmux`, or `screen`
- admin commands `players`, `metrics`, and `stop` are verified on the live host
- save backups are being created as `.bak.json` files in `player_save_dir`
