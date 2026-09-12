# Phase 27 - Testing Tomb: Room B

Room B is a second envelope on the Testing Tomb map, added entirely as module
data. The map grows from 5x5 to 10x5 with a second locked door on Room A's
east wall, and the existing loader, seeding, and door-route machinery resolves
the new location, content, and transitions with zero engine changes.

## Module Data

`maps/tomb.json` now draws the tomb as two envelopes side by side (10 columns
wide, East of the original entrance):

```
##########
#...+....#
##+##....#
#...#....#
##.##....#
```

- the entrance antechamber and Room A stay exactly where they were (columns
  0-4), so every existing test, spawn, treasure, and trap keeps its tile;
- Room A's east wall gains door `module.testing_tomb.door.room_a_to_room_b`
  at `(4, 1)`, locked, routing `room_a` -> `room_b`;
- the right half (columns 5-9) is the Room B burial chamber.

Supporting data all use the existing mechanisms:

- `locations.json` adds `module.testing_tomb.location.room_b` ("Room B",
  description `module.testing_tomb.text.room_b`, same tomb map).
- `text/descriptions.txt` adds a room_b description block.
- `encounters.json` adds `room_b_guard`: 2 `bx.monster.skeleton` at `(6, 1)`.
- `treasures.json` adds `room_b_gold`: 2 gold at `(7, 3)`.
- `traps.json` adds `room_b_pit`: a pit at `(6, 3)` guarding the treasure
  ledge.

## No New Engine Logic

Room B only exercises code that already existed by Phase 26:

- `load_maps` validates the new door (in-bounds, on a door tile, both endpoint
  locations declared) and the new location's `map_id`;
- `load_encounters` / `load_treasures` / `load_traps` validate the Room B
  content positions against the same entry map;
- `GameSession::start_module` plants the second door's locked state, spawns
  and places the skeletons, and seeds the treasure and pit via the same walks;
- `GameSession::resolve_module_location` routes the second door exactly like
  the first, chained Entrance -> Room A -> Room B because both routes are
  per-tile and evaluated in any order.

## Tests

- `GameSession seeds the Room B envelope from module data` — the second door
  is `DoorClosed` + locked on the runtime map, the pit and gold pile are
  planted, and the skeleton guard spawns in Room B.
- `GameSession module door routes trail into Room B and back` — a Thief walks
  Entrance -> (pick) -> Room A -> (pick) -> Room B, roams Room B, then crosses
  both doors back to the Entrance; every crossing moves `current_location_id`
  to the matching declared location. Encounters are cleared and the wandering
  counter is pinned so the walkthrough stays deterministic.
- The existing content-loader tests were extended for the new counts and the
  second door/content entries.

## Validation

- Debug build: warning-free (no C++ changes; data + tests only).
- Doctest: 188/188 cases, 14315/14315 assertions (Debug + sanitizer).
- ctest: PASS (Debug + sanitizer).
- Frontend smoke: boots to town from the pregenerated `Hrothgar` save and
  quits cleanly.

READY for the next Testing Tomb slice: tavern/module rumor tables and rumor
delivery state — the two-room layout proves multi-location routing with no
new engine logic, so the remaining decorative state is rumors and module
flags/campaign save-reload.