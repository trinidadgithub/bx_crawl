# Phase 10 — Module Session Entry

## Summary

Added the first runtime seam for starting a `GameSession` from data-only module
content. A validated `ModuleContent` can now place a supplied hero at the module
entry location and convert immutable module map topology into runtime `Tile`
state.

This remains intentionally narrow. It does not load encounters, traps,
treasures, doors as persistent objects, or campaign save state.

## API

`GameSession` now exposes:

```cpp
struct ModuleStartResult {
    bool ok{false};
    std::string error;
};

ModuleStartResult GameSession::start_module(const ModuleContent& module,
                                            Entity hero);
```

The method:

- Resolves the manifest entry location.
- Resolves that location's map.
- Verifies the map dimensions, entry coordinates, tile count, and entry tile.
- Resets runtime `Game` state.
- Copies module `TileType` values into `Game::map`.
- Places the supplied hero at the module entry coordinate.
- Sets `GameState::Dungeon`.
- Sets `current_location_id` to the module entry location ID.
- Emits a text log and structured `LogMessage` event.

## Compatibility

- No save format/version changed.
- Existing procedural dungeon entry still uses `enter_dungeon(...)`.
- Existing town and dungeon command handling is unchanged.
- No module-specific C++ logic was added for Testing Tomb.
- No module encounters, traps, treasures, or object persistence are active yet.

## Tests Added

Two tests were added:

- `GameSession starts at the Testing Tomb module entry location`
- `GameSession rejects module entry maps that cannot become runtime maps`

The positive test asserts that Testing Tomb starts in dungeon state, positions the
hero at `(2, 4)`, copies floor and door tiles, sets `current_location_id`, moves
carried hero gold into `total_gold` consistently with normal initialization, and
emits both text and structured log events.

## Deferred Work

Still intentionally deferred:

- Frontend command/UI path for selecting or starting a module.
- Module campaign save/reload.
- Door object IDs and mutable door state.
- Encounter, monster, treasure, trap, and module-flag loading.
- Runtime room topology beyond the raw tile map.
- Descriptions rendered through the existing presentation layer.

## Validation

Performed from clean Debug and sanitizer build trees:

- **Debug build**: configure PASS, compile PASS.
- **CTest**: PASS, 1/1 test executable.
- **Doctest**: 152 / 152 test cases, 4881 / 4881 assertions.
- **Sanitizers**: ASan PASS, UBSan PASS, LeakSanitizer PASS.
- **Compiler warnings**: none under `-Wall -Wextra -Wpedantic`.

## Next Readiness

READY for the next Testing Tomb slice: persistent module object IDs for doors and
features, or loading simple encounter definitions that instantiate
`bx.monster.goblin` using existing combat rules.
