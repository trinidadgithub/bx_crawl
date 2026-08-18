# Phase 11 — Module Door Definitions

## Summary

Added data-only module door definitions with stable module object IDs. Testing
Tomb now declares its first persistent door object connecting the entrance to
Room A.

This phase does not change gameplay door behavior. Runtime `Game::map` still
uses tile state for movement and blocking, and module door objects are not yet
persisted in campaign saves.

## Data Shape

`modules/testing-tomb/maps/tomb.json` now includes a `doors` array:

```json
{
  "door_id": "module.testing_tomb.door.entrance_to_room_a",
  "from_location_id": "module.testing_tomb.location.entrance",
  "to_location_id": "module.testing_tomb.location.room_a",
  "x": 2,
  "y": 2,
  "locked": true
}
```

Testing Tomb also now includes `module.testing_tomb.location.room_a` and its
description so the door has concrete location endpoints.

## Loader Changes

Added `ModuleDoorDefinition`:

- `door_id`
- `from_location_id`
- `to_location_id`
- `x`
- `y`
- `locked`

`ModuleContent` now stores:

- `std::map<std::string, ModuleDoorDefinition> doors`
- `const ModuleDoorDefinition* find_door(const std::string& door_id) const`

## Validation

Door loading validates:

- `door_id` exists and is inside `module.<module_id>.door.*`.
- `from_location_id` and `to_location_id` exist in loaded locations.
- Door coordinates are inside the map.
- Door coordinates refer to an actual door tile.
- Duplicate door IDs are rejected.

The `locked` field is optional and defaults to `false` when omitted.

## Tests Added

The Testing Tomb content test now asserts:

- `Room A` loads as a second location.
- `module.testing_tomb.door.entrance_to_room_a` loads.
- The door references entrance and Room A.
- The door coordinates point at `(2, 2)`.
- The door is marked locked.

One new test case was added:

- `Module content loader rejects invalid door definitions`

It covers a door placed on a non-door tile and a door that references a missing
location endpoint.

## Compatibility

- No gameplay behavior changed.
- No save format/version changed.
- No frontend module-start UI was added.
- No module-specific C++ logic was added.
- No mutable door campaign state exists yet.
- No encounter, treasure, trap, or module-flag behavior is active yet.

## Deferred Work

Still intentionally deferred:

- Applying `ModuleDoorDefinition::locked` to runtime lock/pick behavior.
- Persisting door open/closed/locked state by module object ID.
- Updating `current_location_id` when crossing module door/location boundaries.
- Loading room features, encounters, treasures, traps, and flags.
- Save/reload restoration for module object state.

## Validation

Performed from clean Debug and sanitizer build trees:

- **Debug build**: configure PASS, compile PASS.
- **CTest**: PASS, 1/1 test executable.
- **Doctest**: 153 / 153 test cases, 4893 / 4893 assertions.
- **Sanitizers**: ASan PASS, UBSan PASS, LeakSanitizer PASS.
- **Compiler warnings**: none under `-Wall -Wextra -Wpedantic`.

## Next Readiness

READY for a narrow runtime-door integration slice or for data-only room feature
definitions such as Testing Tomb's trapped chest.
