# Phase 25 - Testing Tomb: Scripted Traps

Traps are now live module data. `modules/testing-tomb/traps.json` declares a
pit trap by stable content id at a fixed tile, and `GameSession` plants the
tile trap state at module start. Trigger, saving throw, damage, search, and
disarm resolution all reuse the core rules untouched.

## Module Data (`modules/testing-tomb/traps.json`)

```json
[
  {
    "trap_id": "module.testing_tomb.trap.antechamber_pit",
    "type": "pit",
    "x": 1,
    "y": 3
  }
]
```

A pit sits in the antechamber on a plain floor tile — reachable on foot, clear
of the treasure piles and the goblin guard (which stays sealed behind the
locked door).

## Loader

`ModuleTrap` (`include/module_content.hpp`) and `load_traps`
(`engine/src/module_content.cpp`) mirror the encounter/treasure design:

- Runs only when the manifest declares a `traps` file; otherwise the module
  has no scripted traps. Loads after treasures.
- `trap_id` must live in the module `trap.` namespace.
- `type` must be `pit`, `dart`, or `poison_dart` — the core `TrapType` set.
- Duplicate `trap_id`, out-of-bounds positions, and non-walkable tiles are
  rejected with explicit errors; missing declared file fails as
  `could not open traps`.

## Runtime Seeding

`GameSession::start_module` sets `has_trap = true` and maps the module type to
`TrapType` on the target tile after the map is built, encounters spawn, and
treasures seed. The core rules drive everything else:

- `hero_move` (stepping onto the tile, when not hidden / without a 10-foot
  pole) triggers `trigger_trap`: pit deals `1d6` (or a rope saves the day),
  dart `1d4`, poison dart `1d4` plus a poison saving throw and `1d4` more on
  failure. Damage, death, and log text come from `engine/src/exploration.cpp`
  unchanged.
- `hero_search` reveals a module trap to 10-foot poles, thieves' find-traps,
  and the 1-in-6 non-thief roll, and thieves can disarm it — all existing
  behavior.

## Tests

- `Testing Tomb module loads its scripted traps` — data assertions.
- `Module content loader rejects invalid traps` — all three types load, plus
  missing id/type, invalid type, invalid id namespace, duplicate id, out of
  bounds, non-walkable tile, missing declared file.
- `GameSession seeds tomb traps and reuses the core trigger rules` — seeds the
  pit from data plus a throwaway poison dart on the gold tile; asserts the
  runtime tile state, then steps the hero onto the pit and checks the damage,
  the cleared trap state, and the `You fall into a pit!` log.

## Validation

- Debug build: warning-free.
- Doctest: 183/183 cases, 14204/14204 assertions (Debug + sanitizer).
- ctest: PASS (Debug + sanitizer).
- Frontend smoke: boots to town from the pregenerated `Hrothgar` save and
  quits cleanly.

READY for the next Testing Tomb slice: door state and location transitions —
plant the declared locked door on the runtime map and route the core
open/close commands so crossing it moves the session into Room A.