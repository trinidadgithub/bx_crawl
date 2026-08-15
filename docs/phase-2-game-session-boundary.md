# Phase 2 — GameSession Application Boundary

## Summary

Introduced `GameSession`, a thin application-layer wrapper that owns the
authoritative `Game` and moves gameplay turn sequencing out of the text
frontend, without changing game behavior.

Previously `frontends/text/src/main.cpp` directly coordinated dungeon timing,
initiative, monster turns, the player action, death checks, and peace
evaluation inside its main loop. Now the frontend only submits command intent
through `GameSession::apply(...)` and presents the resulting state. The engine
rules functions are unchanged; `GameSession` delegates to them.

The on-disk save format, all existing engine APIs, and all pre-existing tests
are untouched.

## Architectural Boundary

```text
Text Frontend
    |
    |  command/input intent   (read_player_command / read_town_command)
    v
GameSession
    |
    |  prepare_dungeon_frame()   (dungeon timing + visibility)
    |  apply(PlayerCommand)      (initiative, monster turns, action,
    |                             death checks, peace, state transitions)
    v
existing Game / rules functions     (apply_player_command, apply_town_command,
                                      monsters_turn, roll_initiative, ...)
    |
    v
Game (authoritative mutable state)
```

## GameSession Responsibilities

- Owns the authoritative `Game` by value.
- Executes one logical command cycle per `apply()` call.
- Owns dungeon turn sequencing:
  - initiative (`roll_initiative`),
  - monster turns before and after the player action (`monsters_turn`),
  - the player command (`apply_player_command`),
  - post-action death checks,
  - dungeon-at-peace announcement,
  - dungeon/town state-transition handling (e.g. ascending stairs → camp).
- Owns dungeon time/light/hunger progression via `prepare_dungeon_frame()`
  (wrapping `compute_visibility`), invoked by the frontend when it needs a fresh
  dungeon frame.
- Delegates non-dungeon (town/meta) commands to `apply_town_command`.
- Applies non-turn commands (`Inventory`, `Quit`) without rolling initiative or
  running monster turns, exactly as the old loop did.
- Reports the cycle outcome through a minimal `ActionResult`
  (`keep_playing`, `hero_died`, `died_before_action`).

## Frontend Responsibilities

- Terminal setup / raw mode.
- Keyboard/input parsing (`read_player_command`, `read_town_command`).
- Menu display and help text.
- Rendering (`render(...)` on the session's `Game`).
- Frame preparation trigger (`prepare_dungeon_frame()` before a dungeon render).
- Death message presentation (driven by `ActionResult`).
- Startup save/load and character generation flow.

## Sequencing Moved

All of the following moved out of `frontends/text/src/main.cpp` into
`engine/src/game_session.cpp`:

- **Dungeon timing / light / hunger** — the frontend previously called
  `compute_visibility(g)` directly each dungeon iteration; it now calls
  `GameSession::prepare_dungeon_frame()` (same underlying function). This step
  still happens *before* the next dungeon frame is rendered, preserving the
  original frame timing and visible output exactly.
- **Initiative** — `roll_initiative` is now invoked inside `apply()`.
- **Monster turns** — both the monster-first (pre-action) and monster-last
  (post-action) `monsters_turn` calls are now inside `apply()`.
- **Player action** — `apply_player_command` is now invoked inside `apply()`.
- **Death checks** — all three death checks are now inside `apply()`, which
  reports `hero_died` / `died_before_action` so the frontend can present the
  historical death text ("You have died." / optional "Game over.").
- **Peace announcement** — `announce_dungeon_at_peace` is now invoked inside
  `apply()`.
- **State transitions** — the "did the hero leave the dungeon?" check that used
  to `continue` the loop is now inside `apply()`, which stops the turn cycle
  when `g.state != GameState::Dungeon` after the player action.
- **Town dispatch** — the frontend previously called `apply_town_command`
  directly; it now goes through `apply()`, which delegates.

## Compatibility

- `Game`, `PlayerCommand`, and all engine functions (`apply_player_command`,
  `apply_town_command`, `monsters_turn`, `roll_initiative`, `compute_visibility`,
  etc.) are preserved unchanged. Existing tests and utilities keep calling them
  directly.
- `GameSession::game()` / `game() const` are **transitional accessors**: the
  text frontend still needs direct access for the startup save/load flow,
  character generation, and rendering until a `GameStateSnapshot` presentation
  layer exists (Phase 3). Documented as transitional in
  `include/game_session.hpp`.
- A `GameCommand` wrapper was **not** introduced: `GameSession::apply` accepts
  the existing `PlayerCommand` directly. The follow-up review warned that
  `PlayerCommand::spell_index` is reused as a generic selection index in
  town/shop/equipment flows, so renaming or wrapping it now would add churn
  without a stable semantics gain. The session is the seam; the command type can
  be reconsidered when the command vocabulary stabilizes.

## RNG / Determinism

Seeded behavior is unchanged. `apply()` calls the same engine functions in the
same order the old frontend loop did, including the placement of
`compute_visibility` (now `prepare_dungeon_frame()`) *before* the command cycle
and *before* initiative, so the number and ordering of random rolls are
identical. The new boundary tests verify this by running identical seeded games
through `GameSession` and through the old sequencing and asserting the final
hero, monster, event-log, clock, and RNG stream are indistinguishable.

## Tests Added

Seven new test cases in `tests/test_core.cpp` (130 test cases / 4505
assertions total, up from 123 / 4259):

- `GameSession owns the authoritative Game state`
- `GameSession delegates town commands through a single boundary`
- `GameSession: non-turn commands do not consume initiative or monster dice`
- `GameSession preserves dungeon turn sequencing and RNG ordering` (+ subcases
  for move-into-monster, wait, rest, monster-free floor)
- `GameSession leaves the dungeon and skips monster checks after ascending`
- `GameSession: monsters winning initiative can kill the hero before acting`
- `GameSession.prepare_dungeon_frame advances dungeon timing and lights the hero tile`

## Deferred Work

Explicitly out of scope for Phase 2 and not implemented:

- `GameStateSnapshot` / `TileView` / `HeroView` / `EntityView` and renderer
  extraction (Phase 3).
- Structured `GameEvent` architecture (`AttackHit`, `DamageApplied`, ...).
- Sound decoupling to an event consumer — existing `sound_*` calls remain inside
  rules code.
- Character-generation refactor (`CharacterGenerator`, prompt abstractions).
- `ContentId`, `RulesCatalog`, module loader, campaign-state DTO, Testing Tomb.
- F-003 richer load diagnostics remain **deferred**; the `bool load_game`
  API is unchanged and the frontend retains its generic failure text. Phase 2
  introduced no diagnostic surface.
- Full command-system redesign (`GameCommand` wrapper, structured result types).

## Validation

Performed from a clean build tree:

- **Debug build** (`-DCMAKE_BUILD_TYPE=Debug`): configure PASS, compile PASS.
- **CTest**: PASS, 1/1 test executable.
- **Doctest**: 130 / 130 test cases, 4505 / 4505 assertions.
- **Sanitizers** (`-DBX_ENABLE_SANITIZERS=ON`, Debug): ASan PASS, UBSan PASS,
  LeakSanitizer PASS (`ASAN_OPTIONS=detect_leaks=1`) for both CTest and direct
  doctest runs.
- **Compiler warnings**: none under the `-Wall -Wextra -Wpedantic` baseline.
- **Manual smoke test** (`BX_LOAD_SAVE=Merlin`):
  - app starts, renders town square, quits cleanly;
  - camp → dungeon entry → movement → quit;
  - dungeon with `BX_TEST_MONSTER=Goblin`: initiative rolled and monster turns
    ran through `GameSession` each turn;
  - town → tavern navigation works.
  All scenarios exited 0 with rendering and event-log output appearing unchanged.

## Phase 3 Readiness

READY. `GameSession` now owns gameplay sequencing behind one stable seam, and
the frontend already consumes it. The next incremental step
(`BX-ARCH-003 — Frontend Snapshot and Renderer Separation`) can build snapshots
from `GameSession`'s authoritative `Game` without further turn-sequencing
migration.

Phase 3 has NOT begun.