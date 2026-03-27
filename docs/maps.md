# Map Contract

Phase 0-3 uses a minimal JSON map file that preserves the future Tiled path without requiring a full import pipeline yet.

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

## Tiled Direction

This format is intentionally close to data we can derive from Tiled later:
- `collision` maps cleanly to a named collision layer
- `spawn_points` map cleanly to spawn objects in an object layer
- `interactables` map cleanly to named objects in a future object layer export
- `tiled_hints` stores the expected stable layer/object names for future import tooling

## Early Expectations

- the map must load at startup or the server should refuse to run
- at least one spawn point must exist
- collision dimensions must match `width` and `height`
- world movement validation should depend on this data, including closed door/object state, not hardcoded bounds
