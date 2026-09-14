# Phase 29 - Testing Tomb: Module Flags and Campaign Save/Reload

Phase 28 proved rumor delivery state (heard ids) is mutable campaign state. This
slice makes that state real: the campaign model captures a module's mutable
runtime state (identity, heard rumors, door flags), the binary save format
carries it, and a fresh `GameSession` can re-bind the module from content plus
the captured state so a replayed hero keeps what they earned.

Scope stays "flags", not a world snapshot: monsters and loot re-seed from module
content on resume, exactly like re-entering a procedural dungeon regenerates it.
The runtime map is transient; only the flags a campaign must remember persist.

## Module Identity on Game

`Game` now carries the module it is running:

- `Game::module_id` / `Game::module_version` — set by `start_module` from the
  manifest (`testing_tomb` / `0.1.0`); empty for the procedural core campaign.
- `ModuleDoorFlag{door_id, open, locked}` and `Game::module_door_states` — the
  mutable door flags, keyed by the module's stable door id.

`GameSession` now records `door_id` on its private `ModuleDoorRoute` (previously
discarded) and keeps `Game::module_door_states` in step with the runtime door
tiles: the session syncs them at the end of every dungeon command in module mode
and once after `start_module`, so opening, closing, picking, and forcing through
the core tile rules is reflected in the campaign-facing flags.

## CampaignState

Capture/restore now carries the module fields:

```cpp
std::string module_id;
std::string module_version;
std::string current_location_id;
std::vector<GameRumor>       module_rumors;      // resolved table (mirror)
std::vector<std::string>     heard_rumors;
std::vector<ModuleDoorFlag>  module_door_states;
```

The "empty until module loading exists" reservation is gone: a procedural Game
captures empty identity (and already did per the round-trip tests), while a
module Game captures its identity, heard ids, and door flags. `restore_campaign_state`
writes them back onto Game.

## Save Format v6

`save_game`/`load_game` advance to version 6 (the writer emits a `6` header; the
reader accepts 1-6). After the v5 armor tail, a v6 payload appends:

- `module_id`, `module_version`, `current_location_id` (length-prefixed strings);
- `heard_rumors` as a length-prefixed list of string ids (cap 100);
- `module_door_states` as a length-prefixed list of `{door_id, open:i32, locked:i32}`
  records (cap 100).

Rumor **texts** are not saved: they are content and re-resolve from the module on
resume; only heard-ness is state. Versions 1-5 load with all module fields
defaulted empty, matching the existing forward-compatibility contract (trailing
fields can be ignored by older loaders, and older files open in the new loader).

## Resuming a Campaign

`GameSession::resume_module(module, hero, campaign)` re-binds a module campaign:

1. runs the existing `start_module` (rebuilds the authored map and content, re-resolves
   rumor texts);
2. re-applies `heard_rumors`, `current_location_id`, and the door flags onto the
   freshly rebuilt door tiles (open doors reopen, picked locks stay unlocked,
   untouched doors keep the author's locked state);
3. re-syncs the door flags.

Capturing, binary-save/load, and resuming round-trips end to end: session →
`capture_campaign_state` → `save_game` → `load_game` → capture again →
`resume_module` reproduces the same heard rumors, open doors, and location.

## Tests

- `CampaignState captures module runtime state: identity, rumors, doors` — capture
  and restore faithfully copy identity, the rumor table, heard ids, and door flags.
- `GameSession resume_module restores heard rumors and door flags` — a session that
  heard both rumors and picked the entrance door resumes with both rumors still heard,
  the entrance door open/unlocked, and Room B's door still locked as authored.
- `Module campaign state round-trips save/load and resumes` — the full persistence
  arc: captured campaign → v6 save → load (identity + heard ids + door flags on the
  Game) → re-capture → resume reproduces the session state.
- Crafted v6 payload loads its module fields (`Crafted v6 payload loads module
  campaign fields, validating the v6 tail`).
- `Save/load round-trips module campaign state (v6)` — a Game's module identity,
  location, heard rumors, and door flags survive the binary round-trip.
- `Malformed saves: v6 module fields reject corrupt counts and strings` — negative/
  oversized rumor and door counts, a corrupt module-id length, and truncation
  anywhere in the v6 tail are all rejected without disturbing live state.
- `Old save versions still load with defaulted fields` — extended to version 5 and
  now asserts all module fields default empty for pre-v6 saves.
- The bad-version list drops `6` (now legal) in favor of `7`.

## Validation

- Debug and sanitizer builds: warning-free.
- Doctest: 199/199 cases, 14885/14885 assertions (Debug + sanitizer).
- ctest: PASS (Debug + sanitizer).
- Frontend smoke: boots to town from the pregenerated `Hrothgar` save with
  `printf 'x' | timeout 30 env BX_LOAD_SAVE=Hrothgar ./build/frontends/text/bx_crawl_text`
  and quits cleanly.

READY for the next Testing Tomb slice. The module proof is complete for the
session surface: encounters, treasures, traps, doors, a two-room layout, rumor
delivery, and now campaign save/reload with door flags — all data-driven with no
module-specific engine rules.