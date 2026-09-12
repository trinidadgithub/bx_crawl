# Phase 23 - Testing Tomb: Scripted Encounters

## Summary

The Testing Tomb module now spawns monsters. `encounters.json` names monsters
by stable content id and tile position; `GameSession::start_module` resolves
each id through the rules catalog and instantiates the monster from `bx_data`.
This is the end-to-end proof of the Phase 21/22 content catalog: module data
stays data-only, and the engine maps ids to rules via validated lookups.

## Module Data (`modules/testing-tomb/encounters.json`)

```json
[
  {
    "encounter_id": "module.testing_tomb.encounter.goblin_watch",
    "monster": "bx.monster.goblin",
    "x": 2,
    "y": 1,
    "count": 3
  }
]
```

- `encounter_id` must live in the module `encounter.` namespace.
- `monster` is a stable rules content id (resolved at module start).
- `x`/`y` are validated against the entry map: in bounds and walkable.
- `count` (optional, default 1) spawns additional copies.

## Engine Changes

- `ModuleEncounter` and `ModuleContent::encounters` (`include/module_content.hpp`).
- `load_encounters` in `engine/src/module_content.cpp`: only runs when the
  manifest declares an `encounters` file; otherwise the module simply has no
  scripted encounters. Loads after descriptions so existing validation error
  priority is unchanged.
- `GameSession::start_module` spawns each encounter:
  content id -> `RulesCatalog::find_monster` -> `bx::monster_index` ->
  `spawn_bx_monster_by_index` (same populated-Entity builder the core dungeon
  uses, including treasure rolls).
- Placement prefers the requested tile, then the four neighbors; unplaceable
  encounters and unknown monster ids fail the module start cleanly (game reset
  to its pre-start state, consistent with the map-mismatch failure path).
- `spawn_bx_monster_by_index` is exposed via `game_internal.hpp` (the file
  already wraps global-scope implementations in thin `game_internal::`
  wrappers).

## Validation

- **Build**: `cmake --build build` PASS, no warnings.
- **CTest**: PASS (Debug and sanitizer).
- **Doctest**: 177 / 177 test cases, 14110 / 14110 assertions - 3 new tests
  (encounter loading, invalid encounter rejection, session spawn + unknown
  monster rejection); the existing module-entry test now asserts the three
  goblins spawn at (2,1), (3,1), (1,1).
- **Sanitizers**: full suite PASS.
- **Smoke test**: full frontend chargen -> town path runs.

## Next Readiness

READY for scripted treasures: `treasures.json` placements by stable rule id,
seeded at module start, with pickup state matching the core game.