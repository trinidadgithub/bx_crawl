# Phase 16 - Spell Extraction

## Summary

Moved spell execution out of `engine/src/game.cpp` and into
`engine/src/spells.cpp` without changing spell behavior or the public engine API.

## Moved To `engine/src/spells.cpp`

- `cast_spell(...)`
- spell level lookup by spell name
- cleric spell-name detection
- spell-local undead checks used by spell targeting and immunity

The existing string-chain spell dispatch was preserved intentionally. This phase
is an extraction, not a spell-system redesign.

## Internal Boundary

`engine/src/game_internal.hpp` now also exposes private engine helpers needed by
spell execution:

- log and sound event emission
- hero confusion checks
- monster lookup
- treasure and XP award hooks

The public `include/game.hpp` API remains unchanged.

## Compatibility

- No public API changed.
- No spell behavior changed intentionally.
- `apply_player_command(...)` continues to call `cast_spell(...)` through the
  private engine header.

## Validation

- **Build**: `cmake --build build` PASS.
- **CTest**: `ctest --test-dir build --output-on-failure` PASS.
- **Doctest**: `./build/tests/bx_crawl_tests` PASS, 153 / 153 test cases,
  4893 / 4893 assertions.
- **Sanitizers**: `cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug
  -DBX_ENABLE_SANITIZERS=ON`, build, CTest, and doctest PASS.

## Next Readiness

READY for the next architecture-review slice: extract exploration, time, and trap
handling from `game.cpp` in small behavior-preserving steps.
