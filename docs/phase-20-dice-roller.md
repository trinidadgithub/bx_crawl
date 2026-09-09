# Phase 20 - DiceRoller / RandomService

## Summary

Introduced a `DiceRoller` that owns `Game::rng` and funnels every random draw
for a game through its rule calls. Gameplay code keeps using the plain
`roll_d(Game&, int)` / `rand_range(Game&, int, int)` helpers, which are now thin
wrappers over `DiceRoller::d(sides)` and `DiceRoller::range(lo, hi)`. This gives
deterministic tests and future replay / roll-history support a single hook
point.

## New Rule API (`include/dice.hpp` / `engine/src/dice.cpp`)

- `DiceRoller` wraps a `std::mt19937`:
  - `d(int sides)` - uniform draw in `[1, sides]`.
  - `range(int lo, int hi)` - uniform draw in `[lo, hi]`, inclusive.
- `Game::rng` is now a `DiceRoller`; `CampaignState::rng` mirrors it.
- The old `std::mt19937` surface is preserved on the wrapper (default/copy,
  `seed`, `operator()`, `result_type`, `min`, `max`), so existing seeding,
  snapshot, comparison, and `Game`/`CampaignState` copy code compiles and
  behaves unchanged.
- All `std::uniform_int_distribution` construction now lives in `dice.cpp`; no
  draw path touches the wrapped engine directly.

## Behavior Preservation

- `roll_d(g, sides)` = `g.rng.d(sides)`; `rand_range(g, lo, hi)` =
  `g.rng.range(lo, hi)` - identical distribution construction and draw order per
  call, so every roll sequence is byte-for-byte the same as before.
- `make_minimal_game`, tools (`show_map_features`, `sim_surprise`), and tests
  seed/copy/draw `g.rng` exactly as they did with a raw `std::mt19937`.
- Save/load and session snapshot/restore copy the whole `Game` (and
  `CampaignState`), so the RNG is carried the same way as before.

## Validation

- **Build**: `cmake --build build` PASS, no warnings.
- **CTest**: `ctest --test-dir build --output-on-failure` PASS.
- **Doctest**: `./build/tests/bx_crawl_tests` PASS, 168 / 168 test cases,
  14008 / 14008 assertions - includes 6 new `DiceRoller` cases (seeded
  reproducibility, `d`/`range` bounds, copy independence, operator()-vs-rule
  stream interleaving, `Game::rng` snapshot/replay of `roll_d`); the pre-existing
  162 cases pass unchanged.
- **Sanitizers**: `cmake --build build-sanitize` PASS, CTest PASS, doctest PASS
  (168 / 168 test cases, 14008 / 14008 assertions).
- **Smoke test**: full frontend chargen -> town path runs with the wrapped RNG.

## Next Readiness

READY for the next architecture-review slice: replace fixed-count
weapon/armor/gear arrays with ID-based inventory / validated content indexes.