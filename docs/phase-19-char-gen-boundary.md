# Phase 19 - Character Generation Boundary

## Summary

Split interactive character creation away from the engine. Level-1 generation
rules are now pure, non-interactive helpers in `include/chargen.hpp` /
`engine/src/chargen.cpp`, and every prompt/print lives in the text frontend
(`frontends/text/src/char_session.cpp`). The engine no longer has an
interactive `init_game(Game&, data_dir)` path.

## New Pure Rules API (`include/chargen.hpp` / `engine/src/chargen.cpp`)

No Console I/O:

- `roll_3d6`, `roll_abilities` (all six scores)
- Class menu: `character_class_count`, `character_class_at`
- Class rules: `hit_die_sides`, `damage_die`, `base_ac`, `attack_bonus_base`,
  `prime_requisite_score`, `prime_name`, `xp_bonus`
- Ability modifiers: `strength_to_hit_modifier`, `dexterity_ac_modifier`,
  `constitution_hp_modifier`
- Random resources: `roll_starting_gold`, `roll_starting_hp`
- Gear menus: `class_weapon_indices`, `class_armor_options`,
  `class_can_use_shield`
- Gear buying (equips + deducts purse, or no-ops): `buy_starting_weapon`,
  `buy_starting_armor`, `buy_starting_shield`
- Spells: `class_starting_spells`, `grant_starting_spells`
- Finalize: `finalize_level_1_hero` (purse, flags, thief skills, ammunition)

## Text Frontend (`frontends/text/src/char_session.cpp`)

`create_hero_interactive(Game&)` drives character creation with the same
messages, order, and RNG usage as the old engine `create_hero`:

- name prompt, reroll loop, class menu, stat/gear/spell derivation, hero sheet,
  and the final "Press Enter" pause
- `format_purse` / `price_status` display helpers moved alongside

The roll source is unchanged: character creation still draws every value from
`Game::rng` via the pure rule helpers.

## Engine Init Surface

- Removed `init_game(Game&)` and the interactive `init_game(Game&, data_dir)`.
- Added `start_game_with_hero(Game&, Entity, data_dir)`: attaches a finished
  hero to an already-initialized game without re-seeding the RNG, so character
  rolls and dungeon generation share one seed stream.
- `main.cpp` new-game path: `init_game_runtime` (seed) → `create_hero_interactive`
  → `start_game_with_hero`.
- `init_game(Game&, Entity)` and `init_game(Game&, Entity, data_dir)` (used by
  tests and tools) are unchanged.

## Compatibility

- Interactive prompts and outputs preserved byte-for-byte.
- Weapon/armor/shield buying rules, spell grants, and derived stats are
  identical (rules were moved verbatim).
- No `Game`/public-state coupling added beyond the existing `Game&` access.
- One public change: the interactive engine init entry was replaced by the
  frontend-driven `start_game_with_hero`.

## Validation

- **Build**: `cmake --build build` PASS, no warnings.
- **CTest**: `ctest --test-dir build --output-on-failure` PASS.
- **Doctest**: `./build/tests/bx_crawl_tests` PASS, 162 / 162 test cases,
  5000 / 5000 assertions (new chargen rules coverage added).
- **Sanitizers**: `cmake --build build-sanitize` PASS, CTest PASS, doctest PASS
  (162 / 162 test cases, 5000 / 5000 assertions).
- **Smoke test**: piped frontend input exercises the full interactive path
  (name, reroll, class, weapon, armor, shield, town entry).

## Next Readiness

READY for the next architecture-review slice: wrap `Game::rng` behind a
`DiceRoller` / random-service rule API.