# Phase 17 - Exploration / Time / Traps Extraction

## Summary

Moved visibility, hunger/light timing, wandering-monster timing, trap triggering,
searching, and hero dungeon movement out of `engine/src/game.cpp` and into
`engine/src/exploration.cpp` without changing behavior or the public engine API.

## Moved To `engine/src/exploration.cpp`

- Encumbrance and move-rate timing: `carried_weight_cn`, `exploration_move_feet`,
  `encounter_move_feet`, `ticks_per_exploration_turn`
- Visibility and lighting: `compute_visibility`, `cast_ray`, `is_opaque`
  (including the one-per-B/X-turn torch/lantern/light-spell decrement and hunger
  clock)
- Trap triggering: `trigger_trap`
- Wandering-monster timing: `tick_wander_counter`
- Searching: `hero_search` (traps, secret doors, and noise at doors)
- Dungeon movement: `hero_move` (monster contact, traps, stairs, thief wall
  climbing, move-silently checks) and `update_location_description`

## Internal Boundary

`engine/src/game_internal.hpp` now also exposes private engine helpers shared
between `game.cpp` and `exploration.cpp`:

- `tick_wander_counter`, `hero_search`, `hero_move`
- `spawn_wandering_monsters` (definition stays in `game.cpp`)
- `resolve_first_contact_surprise`, `descend_stairs`, `ascend_stairs` forwarders

The public `include/game.hpp` API remains unchanged.

## Compatibility

- No public API changed.
- `apply_player_command(...)` now delegates the Move and Search cases to
  `hero_move(...)` / `hero_search(...)`.
- Monster spawning stays in `game.cpp`; exploration only consumes a private hook.

## Validation

- **Build**: `cmake --build build` PASS, no warnings.
- **CTest**: `ctest --test-dir build --output-on-failure` PASS.
- **Doctest**: `./build/tests/bx_crawl_tests` PASS, 153 / 153 test cases,
  4895 / 4895 assertions.
- **Sanitizers**: `cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug
  -DBX_ENABLE_SANITIZERS=ON`, build, CTest, and doctest PASS.

## Next Readiness

READY for the next architecture-review slice: extract town state transitions,
show behavior, equipment handling, and inventory helpers from `game.cpp`.