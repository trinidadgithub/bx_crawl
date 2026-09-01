# Phase 12 — Docker Packaging Fix

## Summary

Fixed stale Docker packaging called out by the architecture review.

The Dockerfile now builds the checked-in project sources, copies the actual text
frontend binary, and packages the runtime data needed by the frontend.

## Changes

- Stopped regenerating `include/bx_data.hpp` during Docker builds. The normal
  project build uses the checked-in curated header unless `BX_REGENERATE_DATA` is
  explicitly enabled.
- Copied the actual frontend executable: `bx_crawl_text`.
- Updated `/game/run.sh` to run `./bx_crawl_text`.
- Copied runtime `data/` to `/data`, matching the current frontend fallback data
  lookup from `/game`.
- Copied `modules/` to `/modules` so module fixtures are present in the runtime
  image.
- Copied root documentation files used by the previous image layout.
- Included `tools/` in the builder context so the full default CMake build has
  all configured targets available.

## Validation

- `docker build -f docker/Dockerfile -t bx-crawl:docker-packaging-test .`: PASS.
- Container smoke verified:
  - `/game/bx_crawl_text` exists and is executable.
  - `/data/descriptions.txt` exists.
  - `/data/pregenerated_saves` exists.
  - `/modules/testing-tomb/module.json` exists.
  - Bounded frontend startup produces output without missing packaged files.

## Compatibility

- No gameplay behavior changed.
- No CMake behavior changed.
- No source code changed.
- Docker now follows the same checked-in data-header path as local builds.

## Next Readiness

READY for the next architecture-review-only item: `game.cpp` extraction planning,
`DiceRoller`, or character generation separation.
