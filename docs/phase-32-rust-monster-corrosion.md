# Phase 32 - Rust Monster / Metal Corrosion

B/X gives the rust monster its signature attack: any normal metal it touches
rusts and crumbles. Previously a rust monster's "1 touch (rust)" row hit the
generic melee path (the parsed damage "special" degraded to an ordinary atk-roll
+ 1d6). This slice adds the corrosion rule at the same layer the drains and
weapon immunity landed: a data-driven special flag set at spawn, resolved in
the combat special chain.

## Data at spawn

- `corrodes_metal` on `Entity` is derived from the monster name (`contains
  "Rust"`) in `spawn_bx_monster_by_index`, exactly like `undead`, `has_poison`,
  and the Phase 31 drain flags — no touch to the auto-generated `bx_data.hpp`.
- The combat special chain in `attack_internal` gained an
  `else if (attacker.corrodes_metal)` branch that calls
  `apply_rust_corrosion`, in the same defender-survived spot the drains use.
  No saving throw: B/X rust monster rust has none.

## What is metal

Material classification is new to the engine (no item carries a material field),
so Phase 32 defines it in one place next to the other item rules:

- Metal weapons: battle axe, hand axe, dagger, short sword, sword, two-handed
  sword, mace, pole arm, war hammer.
- Not metal (left alone): the wooden/simple weapons club, sling, crossbow,
  long bow, short bow, and spear (wooden shaft with only a small head), and the
  **silver dagger** — the acceptance criterion protects silver, which is also
  the only weapon-material special the game knows (Phase 31).
- Metal armor: chain mail and plate mail. Leather is not metal.
- The shield is treated as metal (rim/boss), and its wipe-out only clears
  `has_shield`, never moving `ac` (a held shield never adjusted `ac`).

## Subtraction order

Each successful touch corrodes exactly one item, in a fixed priority that B/X
also roughly expresses (the monster eats the metal it reaches):

1. the metal armor worn (chain or plate) — id cleared, `ac` back to 9;
2. the shield;
3. the wielded metal weapon — id cleared, `damage_die` reset to 2 (unarmed
   1d2), mirroring the town sell path;
4. one carried metal weapon, then a carried metal armor/shield not in use.

The deck is deterministic and message-logged per step ("The Rust Monster's
touch rusts your Chain Mail Armor!", "…rusts a nearby Sword!"); a touch with
nothing metal on the hero logs "…rusts no metal to corrode!" and only its
ordinary damage lands. Because corrosion uses the same `weapon_counts` /
`armor_counts` / `weapon_id` / `armor_id` / `has_shield` fields the rest of the
game writes, destroyed equipment also lightens encumbrance and round-trips
through save v7 with no format change.

## Tests

- `Rust Monster gets the corrodes-metal trait at spawn` — `BX_TEST_MONSTER=rust`
  spawns with the flag set; a goblin does not.
- `Rust monster touch corrodes metal equipment`:
  - successive touches eat chain mail, then the shield, then the sword, and
    the fourth touch finds "no metal to corrode" — verifying the ordering and
    that the hero ends unarmed (1d2);
  - silver dagger + leather armor + a carried club are all unharmed, and the
    touch's ordinary damage still lands;
  - a metal sword carried while wielding a club corrodes first ("rusts a
    nearby Sword") with the club untouched;
  - a rusted chain mail stays gone after save/load (ids, `ac`, and counts all
    persist consistently).

## Validation

- Debug and sanitizer builds: warning-free.
- Doctest: 211/211 cases, 15138/15138 assertions (Debug + sanitizer).
- ctest: PASS (Debug + sanitizer).
- Smoke (Debug): Hrothgar and Tomb Runner saves still boot/enter the dungeon
  with exit 0, and `BX_TEST_MONSTER=rust` boots, enters the dungeon, and
  quits cleanly.

Intentionally out of scope: the other corrosion-family monsters. Gray ooze and
ochre jelly already carry their B/X damage strings (2-16 / 2-12); green slime's
dissolve-and-1d8-per-round and the oozes' ignores-armor behavior are a distinct
slice (next on the ROADMAP) that can build on this phase's subtraction and
material-classification machinery. Shadows/wraiths/etc. remain as previously
documented.