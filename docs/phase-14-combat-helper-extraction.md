# Phase 14 — Combat Helper Extraction

## Summary

Started the behavior-preserving combat extraction from `engine/src/game.cpp` by
moving the low-coupling combat helpers into `engine/src/combat.cpp`.

This is intentionally smaller than the full combat extraction because attack,
morale, fleeing, XP, logging, saves, light/confusion, and movement helpers are
currently tightly coupled through `game.cpp` anonymous-scope functions. This
phase creates the combat source file and extracts the safe public helpers first.

## Moved To `engine/src/combat.cpp`

- `resolve_surprise(...)`
- `roll_surprise(...)`
- `monster_surprise_threshold(...)`
- `roll_initiative(...)`
- `any_monsters_alive(...)`
- `announce_dungeon_at_peace(...)`

The public declarations remain in `include/game.hpp`, so callers are unchanged.

## Compatibility

- No public API changed.
- No combat behavior changed intentionally.
- Initiative log text is preserved.
- `GameSession` continues calling the same public functions.
- Full attack resolution and morale remain in `game.cpp` for now.

## Deferred Work

Still required to finish the roadmap combat extraction:

- Move `attack(...)` and `attack_internal(...)`.
- Move bear attack resolution.
- Move morale checks and ally morale-on-death logic.
- Move fleeing combat AI or define a shared exploration/movement helper boundary.
- Decide whether shared internals need a private engine header rather than
  expanding public API.

## Validation

Performed from clean Debug and sanitizer build trees:

- **Debug build**: configure PASS, compile PASS.
- **CTest**: PASS, 1/1 test executable.
- **Doctest**: 153 / 153 test cases, 4893 / 4893 assertions.
- **Sanitizers**: ASan PASS, UBSan PASS, LeakSanitizer PASS.
- **Compiler warnings**: none under `-Wall -Wextra -Wpedantic`.

## Next Readiness

READY for the next combat extraction slice: formalize a small private engine
helper boundary, then move attack/morale implementation into `combat.cpp`.
