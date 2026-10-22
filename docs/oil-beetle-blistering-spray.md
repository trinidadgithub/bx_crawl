# Oil Beetle: Blistering Spray

The Oil Beetle description in Moldvay Basic (B31) specifies a short-range
defensive oil spray requiring an attack roll. Blisters impose -2 to hit for
24 hours; Cure Light Wounds can remove the blisters instead of healing HP.

## Implementation

- `sprays_oil` is assigned at spawn only to `Beetle, Oil`.
- A surviving beetle retaliates after a melee, thrown-weapon, or damaging spell attack by an
  adjacent hero. The spray has its own d20 attack roll against the hero's AC,
  including active defensive spell bonuses. It does not require the provoking
  attack to hit and causes no additional HP damage.
- Short range uses one orthogonally adjacent grid space; distant attacks and
  slain beetles do not trigger a spray. The normal monster turn still handles
  the beetle's bite independently.
- A spray hit sets `blister_turns` to 144 exploration turns (24 hours at ten
  minutes per turn). Additional hits refresh the duration without stacking the
  penalty. Both melee and thrown-weapon hit rolls take the -2 penalty.
- Cure Light Wounds spends its spell slot removing blisters and returns before
  HP healing or other condition removal.
- The condition is mirrored into `HeroView` and displayed on the dungeon status
  line. Like the existing paralysis condition, it is session-only and does not
  change the save format. Its natural expiry uses the dungeon exploration clock.

The runtime monster description now includes the range, attack roll, explicit
penalty, and healing restriction. Damaging spells and burning oil use the same
short-range retaliation rule.

## Verification

Regression coverage includes spawn classification, successful defensive spray
after a missed melee attack, adjacent thrown attacks, missed sprays, range,
slain beetles, melee/ranged penalties, Cure Light Wounds without healing, and
expiry after 144 exploration turns.
