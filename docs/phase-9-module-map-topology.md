# Phase 9 — Module Map Topology

## Summary

Extended the data-only module loader to read a compact tile grid from a module
map file. Testing Tomb now includes a real 5x5 topology with walls, floors, and
a closed door.

This phase still does not start gameplay from a module, instantiate runtime map
state, spawn monsters, trigger doors, or alter save data. It only validates and
exposes immutable module map topology.

## Map Format

`modules/testing-tomb/maps/tomb.json` now includes `tiles`:

```json
{
  "map_id": "module.testing_tomb.map.tomb",
  "width": 5,
  "height": 5,
  "entry_x": 2,
  "entry_y": 4,
  "tiles": [
    "#####",
    "#...#",
    "##+##",
    "#...#",
    "##.##"
  ]
}
```

Current glyphs:

- `#`: wall
- `.`: floor
- `+`: closed door
- `/`: open door
- `<`: stairs up
- `>`: stairs down

## Loader Changes

`ModuleMapDefinition` now stores:

- `std::vector<TileType> tiles`
- `const TileType* tile_at(int x, int y) const`

`load_module_content(...)` validates:

- `tiles` is present.
- Row count matches `height`.
- Every row width matches `width`.
- Every glyph maps to a known `TileType`.
- The entry coordinate is in bounds.
- The entry tile is walkable.

## Tests Added

The Testing Tomb content test now asserts:

- The tile grid has 25 tiles.
- The entry tile is a floor.
- The center connector is a closed door.
- An outer corner is a wall.
- `tile_at(...)` rejects out-of-bounds coordinates.

One new test case was added:

- `Module content loader rejects invalid map topology`

It covers row-width mismatch, blocked entry tiles, and unknown glyphs.

## Compatibility

- No gameplay behavior changed.
- No save format/version changed.
- No `Game` or `GameSession` module runtime was added.
- No module-specific C++ logic was added.
- No encounter, treasure, trap, or door behavior is active yet.

## Deferred Work

Still intentionally deferred:

- Converting `ModuleMapDefinition::tiles` into runtime `Map`/`Tile` state.
- Starting a `GameSession` at a loaded module entry location.
- Door object IDs and persistent door state.
- Loading encounters, treasures, traps, and module flags.
- Monster instantiation and combat integration.
- Save/reload restoration for module map state.

## Validation

Performed from clean Debug and sanitizer build trees:

- **Debug build**: configure PASS, compile PASS.
- **CTest**: PASS, 1/1 test executable.
- **Doctest**: 150 / 150 test cases, 4849 / 4849 assertions.
- **Sanitizers**: ASan PASS, UBSan PASS, LeakSanitizer PASS.
- **Compiler warnings**: none under `-Wall -Wextra -Wpedantic`.

## Next Readiness

READY for the first `GameSession`/runtime seam that starts at a loaded module
entry location, or for the next data-only slice that adds object IDs for doors
and room features.
