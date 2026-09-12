# Phase 22 - Monster and Spell Catalog Lookups

## Summary

Extended the Phase 21 validated content catalog to monsters and spells.
Name-driven rules no longer scan `bx_data` inline; they resolve through the
catalog so module-facing code has a single validated way to turn a stable
name/id into a table position and read the rule data.

## Catalog Additions (`include/item_catalog.hpp` / `engine/src/item_catalog.cpp`)

- `bx::monster_index(name)` - case-insensitive monster name -> position in
  `bx::monsters` (first match, preserving `bx_data` iteration order), else
  `nullopt`.
- `bx::spell_index(name)` - case-insensitive spell name -> position in
  `bx::spells`, same first-match semantics, else `nullopt`.
- `bx::spell_level_by_name(name)` - catalog-backed spell level (0 if unknown).
- `bx::is_class_spell(class_name, spell_name)` - whether the resolved public
  spell entry belongs to the given class.

All four are `noexcept` and validate against the table, so unknown ids are
rejected instead of guessed.

## Conversion

`engine/src/spells.cpp`'s local `spell_level_by_name` and
`is_cleric_spell_name` helpers now delegate to the catalog (`spell_index` +
table read). Semantics are byte-for-byte the same: both previously scanned
`bx::spells` for the first name match, which is exactly what `spell_index`
does — including the "Light" spawn case (Cleric entry precedes the
Magic-User entry, so `is_class_spell("Cleric","Light")` is true in both
implementations).

The `test_monster_index` dev override and encounter-difficulty sampling in
`game.cpp` intentionally keep their existing normalized-name and numeric
semantics (behavior-preserving; they are not hard-coded table positions).

## Validation

- **Build**: `cmake --build build` PASS, no warnings.
- **CTest**: `ctest --test-dir build --output-on-failure` PASS (Debug and
  sanitizer).
- **Doctest**: 174 / 174 test cases, 14077 / 14077 assertions - includes 1 new
  catalog case; all 173 pre-existing cases pass unchanged.
- **Sanitizers**: full suite PASS.
- **Smoke test**: full frontend chargen -> town path runs.

## Next Readiness

READY for the Testing Tomb encounter phase: modules declare monsters by stable
content id and the engine instantiates them via `bx::monster_index`, proving
the catalog end to end.