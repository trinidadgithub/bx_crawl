# Phase 4 — Structured Events and Sound Decoupling

## Summary

Introduced a minimal structured event stream on `Game` and moved gameplay sound
playback out of rules code. Existing text log behavior is preserved: every call
that previously appended to `Game::event_log` still does, and now also emits a
`GameEventType::LogMessage` event.

Sound effects are now queued as `GameEventType::Sound` records by gameplay code
and consumed by the text frontend. This keeps rules deterministic and removes
direct `sound_*` playback calls from `engine/src/game.cpp` without introducing
an event bus or broad event taxonomy.

## Architectural Boundary

```text
existing rules code
    |
    |  log_event(...) / queue_sound(...)
    v
Game::events
    |
    v
GameSession::events()
    |
    v
Text frontend sound consumer
    |
    v
sound_play(SoundEffect)
```

## What Changed

- Added `include/game_event.hpp` with:
  - `GameEventType::LogMessage`
  - `GameEventType::Sound`
  - `SoundEffect` values for the current sound vocabulary.
- Added `std::vector<GameEvent> Game::events`.
- Updated internal `log_event(...)` to append both existing text log entries and
  structured `LogMessage` events.
- Replaced direct gameplay `sound_*()` calls in `engine/src/game.cpp` with
  queued `Sound` events.
- Added `GameSession::events()` as a read-only event stream accessor.
- Added `sound_play(SoundEffect)` as the frontend-facing sound dispatch helper.
- Moved `sound_init()` from runtime game initialization to the text frontend.
- Updated `frontends/text/src/main.cpp` to consume new sound events with a
  cursor after dungeon frame preparation and after each command cycle.

## Preserved Behavior

- Existing `event_log` text remains unchanged and still drives rendering.
- Existing sound generation functions remain unchanged in `engine/src/sound.cpp`.
- Gameplay rules, RNG ordering, persistence format, snapshots, and command
  sequencing are unchanged.
- Sound remains enabled by the text frontend after startup just as before.

## Tests Added

Two focused tests were added:

- `Structured log events preserve existing event_log text`
- `Gameplay sound cues are queued as structured events`

The full suite now contains 134 test cases and 4540 assertions.

## Deferred Work

Still intentionally deferred:

- Rich structured gameplay event taxonomy (`AttackHit`, `DamageApplied`,
  `EntityKilled`, `TrapTriggered`, etc.).
- Event ranges per `ActionResult` or snapshot-integrated event cursors.
- Removing legacy individual `sound_*` functions once all consumers use
  `sound_play` only.
- Moving sound implementation out of the engine target entirely.
- `GameCommand` redesign.
- Character-generation refactor.
- Content IDs, `RulesCatalog`, module loader, Testing Tomb, and campaign-state
  persistence.
- F-003 richer load diagnostics; `bool load_game` remains unchanged.

## Validation

Performed from clean Debug and sanitizer build trees:

- **Debug build**: configure PASS, compile PASS.
- **CTest**: PASS, 1/1 test executable.
- **Doctest**: 134 / 134 test cases, 4540 / 4540 assertions.
- **Sanitizers**: ASan PASS, UBSan PASS, LeakSanitizer PASS.
- **Compiler warnings**: none under `-Wall -Wextra -Wpedantic`.
- **Manual smoke test** (`BX_LOAD_SAVE=Merlin`): town start/quit, town → tavern,
  camp → dungeon → movement → quit, and dungeon turns with
  `BX_TEST_MONSTER=Goblin` all exited 0 with expected rendering/event-log
  output.

## Next Readiness

READY for the next architecture slice: stable content IDs and a rules catalog
adapter, unless validation review identifies Phase 4 follow-up work first.
