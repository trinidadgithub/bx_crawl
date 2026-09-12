# Phase 26 - Testing Tomb: Door State and Location Transitions

Module doors are now live. `GameSession` plants the locked state declared in
`maps/tomb.json` onto the runtime door tile at module start, and the core
open/close commands drive interaction unchanged. Crossing the door tile maps
the session's `current_location_id` between the two declared endpoint
locations.

## Module Data (`modules/testing-tomb/maps/tomb.json`)

The door is already declared beside the tile layout:

```json
"doors": [
  {
    "door_id": "module.testing_tomb.door.entrance_to_room_a",
    "from_location_id": "module.testing_tomb.location.entrance",
    "to_location_id": "module.testing_tomb.location.room_a",
    "x": 2,
    "y": 2,
    "locked": true
  }
]
```

`ModuleDoorDefinition` (door id, both endpoint locations, tile position,
locked) loaded in Phase 22 is now consumed at session start; the map loader had
already validated position, walkability, and `to`/`from_location_id` uniqueness
against the declared locations.

## Runtime Seeding

`GameSession::start_module` clears any prior door routes, then for each declared
door:

- applies `locked` to the matching runtime `Tile` (the map already provides the
  `DoorClosed`/`DoorOpen` tile type),
- records a `ModuleDoorRoute { x, y, from_location_id, to_location_id }`.

The core rules drive everything else, untouched:

- `is_blocked` (`engine/src/map.cpp`) keeps a closed door impassable and an
  open door walkable, so heroes must unlock/open before stepping through.
- The core `Open` command (`engine/src/game.cpp`) handles the locked door:
  thieves pick it with thieves' tools against `skill_pick_locks`, everyone else
  rolls `2 + openDoors` against a `d6` to force it; failure keeps it locked.
  `Close` reverses an opened door.
- `hero_move` still calls `update_location_description` after each step; that
  procedural north/south split applies to generated dungeons only.

## Location Transitions

After each `Move` in a module session, `GameSession::apply` re-applies the
session's `current_location_id` from the door routes:

```cpp
void GameSession::resolve_module_location(const std::string& prev_location) {
    for (const ModuleDoorRoute& door : module_door_routes_) {
        if (game_.hero.pos.x != door.x || game_.hero.pos.y != door.y)
            continue;
        if (prev_location == door.from_location_id) {
            game_.current_location_id = door.to_location_id;
            return;
        }
        if (prev_location == door.to_location_id) {
            game_.current_location_id = door.from_location_id;
            return;
        }
    }
    game_.current_location_id = prev_location;
}
```

A hero standing on the door tile toggles between the two declared endpoint
locations depending on which side they came from; tiles that are not a door
retain the previous module location. The core procedural location update is
ignored for module sessions (their maps have no north/south split), and the
module location survives until the door is crossed again.

## Tests

- `GameSession plants module door state on the runtime map` — the loaded
  `entrance_to_room_a` door is a `DoorClosed` tile with `locked = true` after
  `start_module`, and the session starts in the `entrance` location.
- `GameSession module doors reuse the core locked-open rules` — a Thief with
  thieves' tools and `skill_pick_locks = 100` picks the planted lock via the
  core `Open` command (door becomes `DoorOpen`, unlocked, `You pick the lock
  and open the door.` logged), then `Close` returns it to `DoorClosed`.
- `GameSession module door routes drive location transitions` — with encounters
  cleared (no wandering RNG), a walkthrough from the entry tile shows the
  closed door blocking movement, the pick unlocking it, stepping through into
  `room_a`, roaming Room A, and stepping back into `entrance`.

## Validation

- Debug build: warning-free.
- Doctest: 186/186 cases, 14240/14240 assertions (Debug + sanitizer).
- ctest: PASS (Debug + sanitizer).
- Frontend smoke: boots to town from the pregenerated `Hrothgar` save and
  quits cleanly.

READY for the next Testing Tomb slice: Room B — a second envelope of the same
proof: door state and location transitions are proven, so Room B adds another
map envelope (or extends the tomb map) with its own door that routes into a
second room, without requiring new engine logic.