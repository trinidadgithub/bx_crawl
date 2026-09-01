# Phase 13 — Persistence Extraction

## Summary

Moved binary save/load implementation out of `engine/src/game.cpp` and into
`engine/src/save.cpp` without changing the public API or save behavior.

This addresses the next architecture-review refactoring item: reducing the
responsibility concentration in `game.cpp` behind existing tests.

## Changes

- Added `engine/src/save.cpp`.
- Added `src/save.cpp` to `engine/CMakeLists.txt`.
- Moved save serialization helpers into `save.cpp`:
  - byte-exact read/write helpers
  - string length validation
  - enum decoding
  - versioned save deserialization
  - filename sanitization helper
  - save directory enumeration helper
- Moved public persistence functions into `save.cpp`:
  - `saves_directory()`
  - `save_path_for_hero(...)`
  - `enumerate_saves()`
  - `strip_save_extension(...)`
  - `pregenerated_saves_directory(...)`
  - `enumerate_all_saves(...)`
  - `find_save(...)`
  - `save_game(...)`
  - `load_game(...)`
- Updated `ROADMAP.md` to reflect the current architecture-review track and the
  completed module/Docker/ADR work.
- Marked the persistence extraction item complete in
  `docs/architecture-review-followup.md`.

## Compatibility

- No public declarations changed in `include/game.hpp`.
- No save format/version changed.
- No save/load validation behavior changed intentionally.
- No gameplay behavior changed.
- Existing callers still use the same functions.

## Validation

Performed from clean Debug and sanitizer build trees:

- **Debug build**: configure PASS, compile PASS.
- **CTest**: PASS, 1/1 test executable.
- **Doctest**: 153 / 153 test cases, 4893 / 4893 assertions.
- **Sanitizers**: ASan PASS, UBSan PASS, LeakSanitizer PASS.
- **Compiler warnings**: none under `-Wall -Wextra -Wpedantic`.

## Next Readiness

READY for the next behavior-preserving extraction from `game.cpp`: combat,
spells, exploration/time/traps, or town/inventory flow.
