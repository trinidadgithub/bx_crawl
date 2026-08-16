# Phase 5 — Content IDs and RulesCatalog Adapter

## Summary

Introduced a small stable content-ID wrapper and read-only `RulesCatalog` adapter
over the existing compiled B/X data in `include/bx_data.hpp`.

This phase establishes the lookup seam needed before module work, without
loading modules, changing save data, or migrating gameplay state away from its
current string/count fields.

## Architectural Boundary

```text
existing bx_data.hpp arrays
    |
    v
RulesCatalog::core()
    |
    v
ContentId lookups
```

The adapter gives future code a stable textual ID surface while preserving the
current arrays and rules implementation.

## Content IDs

Added `include/content_id.hpp`:

- `ContentId { std::string value; }`
- `is_valid_content_id(...)`
- `canonical_content_segment(...)`
- `make_content_id(category, local_name)`

Current ID rules are deliberately simple:

- dot-separated lowercase ASCII segments,
- `a-z`, `0-9`, and `_` inside segments,
- no empty segments,
- no uppercase, spaces, or hyphens in final IDs.

Examples:

```text
bx.weapon.long_bow
bx.armor.chain_mail
bx.gear.pole_10ft
bx.spell.magic_user.magic_missile
bx.monster.bat_normal
```

Existing B/X ids/names that contain spaces, hyphens, apostrophes, or punctuation
are canonicalized into underscore-separated segments.

## RulesCatalog

Added `include/rules_catalog.hpp` and `engine/src/rules_catalog.cpp` with:

- `RulesCatalog::core()`
- `find_weapon(ContentId)`
- `find_armor(ContentId)`
- `find_gear(ContentId)`
- `find_spell(ContentId)`
- `find_monster(ContentId)`
- ID enumeration helpers for each current core array

Spell IDs include class segments so duplicate spell names across classes remain
distinct, for example:

```text
bx.spell.cleric.light
bx.spell.magic_user.light
```

Monster IDs are uniquified when current core data contains duplicate names. The
first duplicate keeps the base ID and later duplicates receive deterministic
numeric suffixes, for example:

```text
bx.monster.ghoul
bx.monster.ghoul_2
```

## Compatibility

- `bx_data.hpp` was not modified.
- Current gameplay code still uses existing arrays and string ids.
- Save format and save version are unchanged.
- No `RulesCatalog` ownership or dependency injection was introduced.
- No module loader, module manifest, Testing Tomb, or campaign state model was
  introduced.

## Tests Added

Five focused test cases were added:

- `ContentId validation accepts stable dotted lowercase ids`
- `ContentId canonicalization maps existing content names to stable segments`
- `RulesCatalog resolves core B/X equipment ids`
- `RulesCatalog resolves core spell and monster ids`
- `RulesCatalog exposes ids for all current core arrays`

The full suite now contains 139 test cases and 4747 assertions.

## Deferred Work

Still intentionally deferred:

- Migrating gameplay inventory/equipment state from current string/count fields
  to content-ID-based state.
- Replacing all direct `bx_data.hpp` array access.
- Module manifest loading.
- Testing Tomb.
- Campaign-state persistence.
- Rules data pack loading.
- F-003 richer load diagnostics; `bool load_game` remains unchanged.

## Validation

Performed from clean Debug and sanitizer build trees:

- **Debug build**: configure PASS, compile PASS.
- **CTest**: PASS, 1/1 test executable.
- **Doctest**: 139 / 139 test cases, 4747 / 4747 assertions.
- **Sanitizers**: ASan PASS, UBSan PASS, LeakSanitizer PASS.
- **Compiler warnings**: none under `-Wall -Wextra -Wpedantic`.

## Next Readiness

READY for a narrow module-manifest/data-loading preparation phase or validation
review of the content-ID adapter before module work begins.
