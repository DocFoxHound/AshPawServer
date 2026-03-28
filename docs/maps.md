# Map Contract

The server currently uses a minimal JSON map format for authoritative startup loading, collision, spawn points, and interactables.

## Required Fields

- `map_id`: stable identifier
- `tile_size`: tile width/height in world units
- `width`, `height`: tile grid dimensions
- `collision`: row-major collision grid where `#` blocks movement and `.` is walkable
- `spawn_points`: array of named world-space spawn positions
- `interactables`: array of authoritative world objects with `id`, `type`, `x`, `y`, and optional state/text fields like `blocks_movement`, `is_open`, and `text`

Current supported types:
- `door`: toggles open/closed and can block movement
- `sign`: returns authoritative read text
- `seat`: tracks occupant state authoritatively
- `container`: toggles open/closed authoritatively

## Validation Rules

Hard-fail validation during load:
- the map file must open and parse as JSON
- `collision` row count must match `height`
- each collision row width must match `width`
- at least one spawn point must exist
- interactable `type` must be one of the supported server types

Startup validation warnings and tooling:
- blocked spawn points are detected by map validation tooling
- duplicate interactable IDs are detected by map validation tooling
- out-of-bounds spawn points and interactables are detected by map validation tooling
- the server logs validation warnings at startup through the map validator

Runtime behavior:
- player spawn attempts skip blocked/occupied spawn points
- if no valid spawn location exists, spawning fails instead of placing a player into an invalid tile
- closed doors and occupied seats participate in authoritative movement blocking

## Tiled Direction

This format stays intentionally close to data we can derive from Tiled later:
- `collision` maps cleanly to a collision layer
- `spawn_points` map cleanly to spawn objects in an object layer
- `interactables` map cleanly to named objects in a future object layer export

## Early Expectations

- the map must load at startup or the server should refuse to run
- at least one spawn point must exist
- collision dimensions must match `width` and `height`
- world movement validation should depend on this data, including closed door/object state, not hardcoded bounds
