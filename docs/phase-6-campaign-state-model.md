# Phase 6 — Campaign State Model

## Summary

Introduced `CampaignState`, an in-memory DTO that separates mutable campaign /
runtime state from runtime-only engine configuration. This creates the seam
needed before module saves without changing the current binary save format or
starting module loading.

The current `Game` remains authoritative for execution. `CampaignState` can be
captured from `Game` and restored into an existing `Game` while preserving
runtime configuration such as `data_dir` and loaded descriptions.

## Architectural Boundary

```text
Game (authoritative runtime state)
    |
    | capture_campaign_state(...)
    v
CampaignState (mutable campaign/runtime DTO)
    |
    | restore_campaign_state(...)
    v
Game (existing rules/session execution)
```

## What CampaignState Contains

- Schema and rules identifiers:
  - `schema_version`
  - `rules_version`
- Reserved module identity fields:
  - `module_id`
  - `module_version`
  - `current_location_id`
- Current game-flow state:
  - `GameState`, `previous_state`, `town_submenu`
- Progression and timing:
  - gold, XP, hero level, dungeon level,
  - wandering-monster counter,
  - dungeon tick clock,
  - dungeon-peace flag.
- Deterministic runtime state:
  - `std::mt19937` RNG state,
  - hero state,
  - map tiles,
  - monsters,
  - rooms,
  - recent event log text.

## What It Does Not Contain

- `DescriptionStore`
- `data_dir`
- transient structured event delivery records (`Game::events`)
- frontend settings
- loaded module definitions
- immutable rules data

Those remain runtime/application concerns and are intentionally not captured in
the DTO.

## Compatibility

- `Game` layout and execution behavior are unchanged.
- The current binary save format and version are unchanged.
- `save_game` / `load_game` are unchanged.
- No module loader, module manifest, Testing Tomb, JSON campaign saves, or
  campaign persistence migration was introduced.

## Tests Added

Three focused test cases were added:

- `CampaignState captures mutable campaign and runtime state`
- `CampaignState restores Game while preserving runtime configuration`
- `CampaignState separates future module identity from current procedural saves`

The full suite now contains 142 test cases and 4790 assertions.

## Deferred Work

Still intentionally deferred:

- Persisting `CampaignState` to disk.
- Migrating the existing binary hero/meta save path to this DTO.
- JSON campaign saves.
- Module manifest loading.
- Testing Tomb.
- Door/trap/treasure/module-object IDs.
- Replacing procedural dungeon state with module location state.
- F-003 richer load diagnostics; `bool load_game` remains unchanged.

## Validation

Performed from clean Debug and sanitizer build trees:

- **Debug build**: configure PASS, compile PASS.
- **CTest**: PASS, 1/1 test executable.
- **Doctest**: 142 / 142 test cases, 4790 / 4790 assertions.
- **Sanitizers**: ASan PASS, UBSan PASS, LeakSanitizer PASS.
- **Compiler warnings**: none under `-Wall -Wextra -Wpedantic`.

## Next Readiness

READY for a narrow module-manifest loader phase. `CampaignState` now gives that
future loader a place to record selected module identity without changing game
execution or persistence yet.
