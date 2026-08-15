# Phase 3 — Frontend Snapshot and Renderer Separation

## Summary

Introduced a frontend-neutral `GameStateSnapshot` presentation DTO and routed the
text frontend renderer through `GameSession::snapshot()` instead of rendering
directly from raw `Game` state.

The text frontend now follows:

```text
GameSession
    |
    v
GameStateSnapshot
    |
    v
Text renderer
```

The existing `Game` model, rules functions, command sequencing, persistence,
and save format are unchanged.

## Architectural Boundary

- `GameSession` remains the application boundary from Phase 2.
- `GameSession::snapshot()` returns a copyable read-only `GameStateSnapshot`.
- `frontends/text/src/render_snapshot.cpp` consumes `GameStateSnapshot` and owns
  terminal text/ANSI rendering for the text frontend.
- `frontends/text/src/main.cpp` still owns terminal setup and keyboard input,
  but it no longer calls `render(const Game&)`.

## Snapshot Contents

The snapshot contains only presentation-facing data needed by the current text
renderer:

- `TileView`: tile type, visibility/exploration flags, visible trap state,
  item stack, and precomputed wall glyph data.
- `EntityView`: monster position, alive flag, display glyph, name, and type.
- `HeroView`: hero identity, class name, position, HP/AC/combat stats,
  equipment counts, supplies, spell names, light/hunger indicators, and
  precomputed status flags.
- `GameStateSnapshot`: game state, dimensions, town submenu, progression,
  movement/encumbrance values, purse text, alive monster count, and event log.

Rendering-specific geometry helpers such as wall visibility and wall glyphs are
computed by the engine while constructing the snapshot so the frontend does not
need private map/rules helpers.

## Renderer Changes

- Added `frontends/text/src/render_snapshot.hpp` and `.cpp`.
- Moved the text rendering path used by the executable into the frontend.
- Updated `frontends/text/src/main.cpp` to call `render(session.snapshot())`.
- Kept rendering output intentionally equivalent to the legacy engine renderer.

## Compatibility

- Legacy `render(const Game&)` remains in `engine/src/game.cpp` and
  `include/game.hpp` for compatibility with existing tests/tools during this
  transitional slice.
- Existing direct engine tests are preserved.
- Startup save/load, character generation, and input mapping still use the
  transitional `GameSession::game()` accessor where needed.
- No `GameCommand`, structured event system, module loader, Content IDs,
  sound-event consumer, or renderer framework was introduced.

## RNG / Gameplay Determinism

No gameplay behavior or RNG ordering changed. Snapshot construction copies and
precomputes presentation state only; it does not roll dice, mutate `Game`, or
advance clocks. Tests verify that taking a snapshot leaves the RNG stream
unchanged.

## Tests Added

Two focused snapshot tests were added:

- `GameSession snapshot exposes presentation state without mutating RNG`
- `GameStateSnapshot carries town and character-sheet render data`

The full suite now contains 132 test cases and 4535 assertions.

## Deferred Work

Still intentionally deferred:

- Removing the legacy engine `render(const Game&)` API after all callers are
  migrated and compatibility risk is low.
- Further shrinking direct frontend reads of `Game` for startup save/load and
  input-menu decisions.
- Structured `GameEvent` output.
- Sound decoupling to an event consumer.
- Character-generation refactor.
- `GameCommand` redesign.
- Content IDs, `RulesCatalog`, module loading, Testing Tomb, and campaign-state
  persistence.
- F-003 richer load diagnostics; `bool load_game` remains unchanged.

## Validation

Performed from clean Debug and sanitizer build trees:

- **Debug build**: configure PASS, compile PASS.
- **CTest**: PASS, 1/1 test executable.
- **Doctest**: 132 / 132 test cases, 4535 / 4535 assertions.
- **Sanitizers**: ASan PASS, UBSan PASS, LeakSanitizer PASS.
- **Compiler warnings**: none under `-Wall -Wextra -Wpedantic`.
- **Manual smoke test** (`BX_LOAD_SAVE=Merlin`): town start/quit, town → tavern,
  camp → dungeon → movement → quit, and dungeon turns with
  `BX_TEST_MONSTER=Goblin` all exited 0 with expected rendering/event-log output.

## Phase 4 Readiness

READY for a future phase that removes the legacy engine renderer and continues
reducing transitional raw-`Game` access, or for the next planned architecture
slice after validation review.
