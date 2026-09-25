# Phase 34 - Slime Fire Immunity and Splitting

B/X gives green slime a different combat rule from ordinary monsters: weapons
pass through it, and only fire or fire-based magic can destroy it. This phase
adds that missing fire path and uses it for the slime-family split rule that the
previous green-slime contact phase left for the next slice.

Scope note: the earlier ROADMAP wording named lightning and black pudding too,
but the phase originally had no lightning damage source and no Black Pudding
monster row. The playable subset started with flaming oil and Fire Ball as fire
sources, green slime as fire-only, and base ochre jelly splitting into the
existing smaller `Ochre Jelly'` row. Lightning Bolt now provides the lightning
source; black pudding remains future data work.

## Fire Sources

- New `PlayerCommandType::ThrowOil`, bound to dungeon key `n` in the text
  frontend, throws a burning oil flask up to four tiles in a straight line.
- The throw consumes both the `oil_flasks` counter and the `oil-flask` gear
  entry, mirroring the lantern-fueling representation.
- Burning oil deals 1d8 fire damage. A normal kill awards treasure/XP like holy
  water and thrown weapons.
- Fire Ball now routes through the same fire-damage helper. Its existing
  behavior of not awarding XP/treasure is preserved.

## Green Slime: Weapons Pass Through

Green slime now spawns with `WeaponImmunity::FireOnly`. The existing melee and
ranged weapon gates reject all weapons, including silver, and log "Your weapon
passes right through the Green Slime!" Spells and burning oil do not use the
weapon gate, so they can still damage it.

This does not change Phase 33's contact rule: a green slime touch still dissolves
non-magical worn gear and clings until scraped off. Silver still survives the
contact-dissolve step, but silver no longer wounds the slime itself.

## Ochre Jelly: Energy Splitting

The base `Ochre Jelly` row now spawns with `splits_on_fire_or_lightning`. If
fire damage from burning oil or Fire Ball, or lightning damage from Lightning
Bolt, would reduce it to 0 HP, the original monster is marked dead and two
`Ochre Jelly'` offspring are queued at the same position. Each gets half the
parent jelly's pre-hit HP, minimum 1, and the split is not a kill, so no
treasure/XP is awarded.

The smaller `Ochre Jelly'` row does not carry the split flag, which keeps this
slice bounded and avoids recursive split storms.

## Tests

- Spawn checks verify green slime receives `FireOnly` and base ochre jelly
  receives `splits_on_fire_or_lightning`.
- Green slime rejects melee club, silver dagger, and thrown knife attacks with a
  "passes right through" message.
- Burning oil rejects missing directions and missing oil, consumes oil on a real
  throw, destroys green slime, and kills ordinary monsters normally.
- Burning oil splits an ochre jelly instead of awarding XP, while a mundane
  melee kill does not split it.
- Fire Ball destroys green slime and splits an ochre jelly in one cast.
- Lightning Bolt is a lightning source and splits an ochre jelly while killing
  ordinary monsters normally.

## Validation

- Debug build: warning-free.
- Sanitizer build: warning-free.
- Doctest: 219/219 cases, 15223/15223 assertions (Debug + sanitizer).
- ctest: PASS (Debug + sanitizer).
- Smoke (Debug): Hrothgar boots/quits with exit 0; with `BX_LOAD_SAVE=Hrothgar`,
  `BX_TEST_MONSTER=green`, `ochre`, and `rust` enter the dungeon and quit with
  exit 0.
