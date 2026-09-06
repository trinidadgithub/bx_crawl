# Phase 15 - Combat Attack Extraction

## Summary

Completed the behavior-preserving combat extraction started in Phase 14 by moving
attack resolution, morale checks, bear attacks, and fleeing combat AI out of
`engine/src/game.cpp` and into `engine/src/combat.cpp`.

## Moved To `engine/src/combat.cpp`

- `attack(...)`
- `attack_internal(...)`
- bear multi-attack and hug resolution
- `try_morale_check(...)`
- ally morale checks after deaths
- `flee_ai(...)`

## Internal Boundary

Added `engine/src/game_internal.hpp` as a private engine-only bridge for helpers
that are still owned by `game.cpp`, such as logging, sound events, saving throws,
XP/treasure awards, confusion/light penalties, morale calculation, occupancy, and
movement.

This keeps the public `include/game.hpp` API unchanged while allowing combat code
to live in its own translation unit. The bridge is intentionally temporary and can
shrink as exploration, spells, progression, and event dispatch are extracted.

## Compatibility

- No public API changed.
- No combat behavior changed intentionally.
- Existing call sites in `game.cpp` still call `attack(...)`,
  `try_morale_check(...)`, and `flee_ai(...)` through the private engine header.

## Validation

- **Build**: `cmake --build build` PASS.
- **CTest**: `ctest --test-dir build --output-on-failure` PASS.
- **Doctest**: `./build/tests/bx_crawl_tests` PASS, 153 / 153 test cases,
  4893 / 4893 assertions.
- **Sanitizers**: `cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug
  -DBX_ENABLE_SANITIZERS=ON`, build, CTest, and doctest PASS.
- **Diff hygiene**: `git diff --check` PASS.

Note: an initial parallel `ctest` plus direct doctest run failed because both
processes raced on shared `/tmp/opencode` module-loader fixture paths. Sequential
reruns passed.

## Next Readiness

READY for the next architecture-review slice: extract spell execution from
`game.cpp` into `engine/src/spells.cpp`.
