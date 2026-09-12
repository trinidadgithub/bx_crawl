# Phase 21 - Inventory Content IDs

## Summary

Introduced a validated content-index catalog for inventory items and ID-based
inventory access, so count mutations no longer rely on magic `bx_data` array
positions. The count arrays on `Entity` stay as storage (save format, snapshot
DTO, and existing tests are unchanged); all kit-writing now funnels through
stable item content ids.

## Validated Catalog (`include/item_catalog.hpp` / `engine/src/item_catalog.cpp`)

- `bx::weapon_index(id)`, `bx::armor_index(id)`, `bx::gear_index(id)` map a
  stable item content id to its canonical position in the matching `bx_data`
  table, returning `std::nullopt` for unknown ids.
- `bx::inventory_item_by_id(id)` looks across all three tables and returns a
  `InventorySlot { kind, index }`.
- `static_assert`s pin `Entity::weapon_counts` / `armor_counts` / `gear_counts`
  sizes to `bx::weapons` / `armor_items` / `gear_items`, so a future table
  change desyncs the build instead of the arrays.
- Unknown ids are rejected, so module-defined items can register later without
  callers assuming a magic array position.

## ID-Based Inventory (`include/inventory.hpp` / `engine/src/inventory.cpp`)

- `item_count(e, id)`, `give_item(e, id, amount)`, `set_item_count(e, id, n)`,
  `consume_owned_item(e, id, amount)` work across weapons, armor, and gear.
  Unknown ids are no-ops (0 / false with no mutation).

## Call-Site Conversions (behavior-preserving)

- `chargen.cpp buy_starting_shield`: shield cost and AC bonus now come from the
  catalog-resolved armor table entry instead of hard-coded `10 gp` and
  `armor_counts[3]` / `ac -= 1`.
- `game.cpp` torch lighting: consumes via `item_count`/`consume_owned_item`
  ("torches") instead of `gear_counts[15]`.
- `town.cpp` gear shop purchase: grants via `give_item(g.hero, item.id)`.
- `save.cpp` legacy migration: shield restored via `armor_index("shield")`.
- `tools/gen_pregenerated.cpp`: replaces its local index helpers and magic gear
  indices with catalog lookups; regenerated output is byte-identical to the
  previous build of the tool.

## Validation

- **Build**: `cmake --build build` PASS, no warnings.
- **CTest**: `ctest --test-dir build --output-on-failure` PASS.
- **Doctest**: 173 / 173 test cases, 14048 / 14048 assertions - includes 5 new
  catalog / ID-inventory cases; all 168 pre-existing cases pass unchanged.
- **Sanitizers**: `cmake --build build-sanitize` PASS, CTest PASS, doctest PASS
  (173 / 173 test cases, 14048 / 14048 assertions).
- **Smoke test**: full frontend chargen -> town path runs.
- **Pregen tool**: `gen_pregenerated` output verified byte-identical to the
  pre-change tool build (the committed `Hrothgar.dat` drift pre-exists at HEAD
  and is outside this phase).

## Next Readiness

READY for the next architecture-review slice: extend content-id lookup to
monsters and spells so rules stop relying on `bx_data` array positions there
too.