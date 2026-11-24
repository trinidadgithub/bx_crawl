# Armor Class Consistency and Physical Shields

Validated local fix on `fix/armor-class-consistency`, base `d8d7f96` (PR #70).
Validation recorded 2026-11-21 UTC. Subsequent commit/push was authorized for
playtesting; checkpoint commit subject: `Fix ascending armor class and shield
defense` (resolve its SHA through Git history). No merge is claimed.

## Cause

Combat compared `d20 + attack_bonus >= defender.ac` (ascending AC), but body
armor and spawned monsters copied descending B/X values directly into that
field. Character creation additionally subtracted a shield from stored AC,
while later equipment changes only changed `has_shield`. Better armor could
therefore make a character easier to hit, and shield removal could leave stale
or missing defense. This was more than a display bug.

The owner approved a full AC-convention correction after this discrepancy was
identified: ascending internally, conversion at the B/X data boundary, labelled
display, and reviewed save compatibility. The approved scope includes armor,
shields, Dexterity, combat targets, and display consistency.

## Convention and Shared Calculation

`include/armor_class.hpp` is the shared boundary/calculation:

- Convert source AC once: **ascending AC = 19 - descending B/X AC**.
- `Entity::ac` stores ascending body/natural AC, with hero Dexterity already
  applied, but without a physical shield or spell buffs.
- Physical shields contribute **+1** derived defense, once, if the equipped
  weapon permits a shield. An unarmed character can use a shield; a two-handed
  weapon cannot receive its defense.
- Snapshots expose equipped/unbuffed AC in `HeroView::ac` and total current
  defense in `HeroView::effective_ac`. The character sheet uses effective AC.
- Normal melee, bear attacks, thrown weapons and oil-spray hit rolls use the
  same effective calculation as the displayed snapshot.

With average Dexterity and no spell buffs:

| Equipment | Original B/X descending | Game ascending |
|---|---:|---:|
| No armor | 9 | 10 |
| Leather | 7 | 12 |
| Chain mail | 5 | 14 |
| Chain mail + shield | 4 | 15 |
| Plate mail | 3 | 16 |
| Plate mail + shield | 2 | 17 |

The game labels its AC as ascending; higher is better. `show_monster` continues
to report the original table as `AC (B/X)` and explains the descending convention
in help. Source `bx_data.hpp`/JSON armor and monster values are not rewritten.

Character creation, buying/equipping/selling body armor, armor removal,
rust/slime destruction, and pregenerated-hero creation now use the same
conversion/DEX convention. Body-armor selection no longer lists Shield as a
body armor: shields already have a separate starting purchase. Repeated shield
equip/unequip and armor replacement cannot accumulate shield bonuses.

## Save Compatibility: Version 8

Version 8 marks the corrected meaning of the existing saved AC field: ascending
base including DEX, excluding shield/buffs. The field layout is otherwise the
v7 layout; this is a semantic version bump, not a new poison-state payload.

- Versions 1-7 still load. Their AC is rebuilt from body-armor ID and saved
  Dexterity, ignoring the ambiguous old AC number. This handles descending,
  ascending-template, and baked-in-shield variants without guessing from a
  number's range.
- Empty/`none` armor IDs become unarmored defense. A legacy `armor_id=shield`
  is normalized to the separate shield slot. A known two-handed weapon removes
  incompatible legacy shield use. Unknown legacy armor IDs reject rather than
  silently inventing a migrated defense; loading remains commit-on-success.
- This reconstruction cannot preserve ad hoc old AC adjustments that were never
  represented separately in state. Supported equipment/DEX are authoritative.
  Existing source flags and spell counters remain separately represented.
- Version 8 loads its stored base AC directly: no second conversion or baked-in
  shield. Repeated save/load does not drift. The original file is not rewritten
  just by loading; migration is written when the player explicitly saves.
- Older executables supporting only v7 reject new v8 saves. Keep a copy of an
  older campaign file before saving with the new build if a downgrade is needed.
  No user/pregenerated save was regenerated or overwritten by this work.
- Future poison-state persistence must use the next available format revision
  (candidate v9), not reuse v8 for a different wire contract.

## Scope Boundaries

This fixes the AC convention and shield/equipment integration. It does not
replace the engine's approximate attack bonuses with full original attack
matrices, implement alignment qualification, or change spell-specific rules.
Existing Shield/Protection/Resist Cold AC bonuses are centralized with their
existing values; this is not a new RAW audit of those spells. No monster catalog
rows, poison profiles, or save treatment states were added.

## Verification

Executed:

```sh
cmake --build build -j 2
./build/tests/bx_crawl_tests
ctest --test-dir build --output-on-failure
cmake --build build-sanitize -j 2
ctest --test-dir build-sanitize --output-on-failure
```

- Debug: **244/244 cases, 16251/16251 assertions**; CTest 1/1, 0.72 seconds.
- ASan+UBSan build and CTest 1/1, 5.18 seconds; no sanitizer errors reported.
  The initial build hit a 120-second compilation timeout; it succeeded when
  resumed with a 300-second tool timeout. Cache verified sanitizers ON and
  regeneration OFF. Existing root-test description-load warnings remain.
- Original regression expectations that encoded descending runtime AC were
  corrected to the new explicit convention. Shared test heroes now have neutral
  STR/DEX rather than uninitialized zero scores.
- New cases exercise equipment/DEX/shield/buff composition, actual hit-versus-miss
  boundaries for shields, bear/oil defenses, one-time monster conversion, starting
  versus later equipping, repeated shield changes, v1-v7 migration and v8 reload,
  and invalid legacy armor with receiving-state preservation.
- A text frontend smoke loaded the shipped Hrothgar template under an isolated
  HOME and sent `b25ttx`. Assertions verified displayed ascending AC **15 -> 14**
  and `Unequipped shield.`; exit 0. No save command was sent.

Review this change before merging. A code rollback must not silently downgrade
v8 saves or overwrite original campaign files. Full poison Phase 1 sign-off
still requires Q08; this playtest correction does not resolve alignment.
