# Phase 18 - Town / Inventory Extraction

## Summary

Moved town/menu state transitions, shop flows, and equipment/inventory rules out
of `engine/src/game.cpp` and into `engine/src/town.cpp` /
`engine/src/inventory.cpp` without changing behavior or the public engine API.

## Moved To `engine/src/town.cpp`

- `apply_town_command` (all town states): Character Sheet, Town, Tavern, Temple,
  Shop, Armorer, and Camp
- Local helpers: `open_character_sheet`, `return_from_character_sheet`,
  `buy_gear_item`, and the per-state `set_state` navigation lambda

## Moved To `engine/src/inventory.cpp`

- `is_weapon_allowed` (global class/weapon restriction rule)
- `game_internal::parse_die_size`
- `game_internal::can_use_shield`
- `game_internal::can_wear_heavy_armor`
- `game_internal::equip_owned_weapon`
- `game_internal::equip_owned_armor`
- Local helper: `current_weapon_two_handed`

## Internal Boundary

`engine/src/game_internal.hpp` now also declares the equipment and armor-rule
helpers shared between `game.cpp`, `town.cpp`, and `inventory.cpp`:

- `parse_die_size`, `can_use_shield`, `can_wear_heavy_armor`,
  `equip_owned_weapon`, `equip_owned_armor`

The public `include/game.hpp` API remains unchanged. `apply_town_command`,
`save_game`, `save_path_for_hero`, and `enter_dungeon` stay public-declared and
are now consumed by `town.cpp` across the same translation-unit split used in
Phases 15-17.

`engine/CMakeLists.txt` gains `src/town.cpp` and `src/inventory.cpp`.

## Compatibility

- No public API changed.
- `game.cpp` retains `format_coin_purse`, `format_price_with_purse`, and
  `class_name` (still used by rendering and character generation).
- Navigation, shop buying/selling, temple donation, and camp save behavior are
  preserved byte-for-byte in their new homes.

## Validation

- **Build**: `cmake --build build` PASS, no warnings.
- **CTest**: `ctest --test-dir build --output-on-failure` PASS.
- **Doctest**: `./build/tests/bx_crawl_tests` PASS, 153 / 153 test cases,
  4895 / 4895 assertions.
- **Sanitizers**: `cmake --build build-sanitize` PASS, CTest PASS, doctest PASS
  (153 / 153 test cases, 4895 / 4895 assertions).

## Next Readiness

READY for the next architecture-review slice: refactor the character generation
boundary so prompts stay in the text frontend.