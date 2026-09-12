# Phase 24 - Testing Tomb: Scripted Treasures

Treasure is now live module data. `modules/testing-tomb/treasures.json` places
gold and gem piles by stable content id at fixed tiles, and `GameSession`
seeds them onto the runtime map at module start. Pickup reuses the core
`Get` command unchanged, so the hero gains the same coins, XP, sound, and
log messages as the procedural dungeon.

## Module Data (`modules/testing-tomb/treasures.json`)

```json
[
  {
    "treasure_id": "module.testing_tomb.treasure.antechamber_gold",
    "type": "gold",
    "x": 2,
    "y": 3,
    "count": 2
  },
  {
    "treasure_id": "module.testing_tomb.treasure.antechamber_gems",
    "type": "gem",
    "x": 3,
    "y": 3,
    "count": 1
  }
]
```

Two piles sit in the antechamber outside the locked door (the goblin watch is
sealed inside Room A, so the hero can safely reach and pick them up).

## Loader

`ModuleTreasure` (`include/module_content.hpp`) and `load_treasures`
(`engine/src/module_content.cpp`) mirror the encounter design:

- Runs only when the manifest declares a `treasures` file; otherwise the
  module simply has no scripted treasure.
- Loads after encounters so existing validation error precedence holds.
- `treasure_id` must live in the module `treasure.` namespace.
- `type` must be `gold` or `gem` — these map directly onto the core tile
  `ItemType::Gold` / `ItemType::Gem` piles.
- `count` defaults to 1 and must be at least 1.
- Duplicate `treasure_id`, out-of-bounds positions, and non-walkable tiles are
  rejected with explicit errors.
- Missing declared file fails as `could not open treasures`.

## Runtime Seeding

`GameSession::start_module` pushes `count` items of the pile type onto the
target tile's `items` vector after the map is built and encounters spawn. No
value is rolled at seeding: the core `Get` command resolves each gold pile as
`10 + d20` gp and each gem as `50 + d100` gp, adds both to `total_gold` and
`total_xp`, plays the pick-up sound, and logs the find — pickup state therefore
matches the core game exactly.

## Tests

- `Testing Tomb module loads its scripted treasures` — data assertions.
- `Module content loader rejects invalid treasures` — missing id/type, invalid
  type, invalid id namespace, zero count, duplicate id, out of bounds,
  non-walkable tile, missing declared file.
- `GameSession seeds tomb treasures and reuses the core Get rules` — asserts
  the seeded tile items, teleports the hero onto the gold and gem tiles,
  issues `Get` through the session turn cycle, and checks the coin ranges and
  the find log.

## Validation

- Debug build: warning-free.
- Doctest: 180/180 cases, 14164/14164 assertions (Debug + sanitizer).
- ctest: PASS (Debug + sanitizer).
- Frontend smoke: boots to town from the pregenerated `Hrothgar` save and
  quits cleanly.

READY for the next Testing Tomb slice: `traps.json` placements with the core
trap/saving-throw resolution on tile entry.