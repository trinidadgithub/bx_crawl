# Phase 30 - Testing Tomb: Module Campaign Boot

Phases 23-29 proved the whole module surface at the engine layer (encounters,
treasures, traps, doors, a two-room layout, rumor delivery, and campaign
save/reload with door flags), but the frontend had never started a module. This
slice turns the proof into a playable campaign: a Testing Tomb campaign save is
produced through the real boot path (`start_module` → play → `save_game`), the
frontend recognizes and re-binds module campaign saves on load, and a hero can
save anywhere in the dungeon and pick the campaign back up at the exact tile
where they stopped.

Scope stays the same flags-not-world-snapshot contract from Phase 29: monsters
and loot re-seed from module content on resume. Position now persists (save
format v7) so a saved campaign resumes in-place; it is a flag, not the runtime
map.

## Finding an Installed Module

`gen_pregenerated` needs to produce a module save, and the frontend needs to
re-bind one on boot, so both need to locate the module's content at a data
location the product ships. Module content remains authored and tested at
`modules/testing-tomb`, and a runtime copy now ships at `data/modules/testing-tomb`
(matching how other runtime data lives under `data/`).

- `find_module_dir(data_dir, module_id)` (in `engine/src/module_manifest.cpp`)
  scans `data_dir/modules/*/module.json` and returns the directory whose
  manifest declares `module_id`. Directory names carry no contract; the
  manifest id is authoritative. It returns an empty path for unknown ids,
  empty ids, or a missing `modules/` root, and never throws (directory
  iteration uses `std::error_code` overloads like `append_dat_files`).

## Save Format v7: Hero Position

A module campaign resuming at the entry tile would be playable but strange — a
hero saved mid-run should come back where they saved. The module map is
deterministic authored content, so the hero's coordinates stay meaningful after
the rebuild; the previous format simply never wrote them.

`save_game`/`load_game` advance to version 7 (the writer emits a `7` header; the
reader accepts 1-7). After the v6 door list, a v7 payload appends the hero's
`pos.x` / `pos.y` as two int32 values. Versions 1-6 load with the position
defaulted to `{0, 0}`, and any file truncated inside the new tail is rejected
without disturbing live state. The reader's trailing-byte tolerance remains:
future fields appended after the position stay compatible with older loaders.

`GameSession::resume_module` re-applies the saved position after the rebuild,
guarded so a position that is out of bounds or lands on a wall (the `{0, 0}`
default a pre-v7 module save carries) falls back to the module entry tile
instead of stranding the hero.

## Dungeon Save Command

Previously a game could only be saved from camp (the camp Help key). A module
campaign never reaches camp, so saving mid-dungeon is the enabler for "play,
save, quit, reload".

- `PlayerCommandType::Save` is a dungeon non-turn command: in
  `GameSession::apply` it rides the same under-the-initiative path as
  `Inventory` and `Quit`, so saving never advances the clock or lets monsters
  act, and it is allowed while the hero is paralyzed or unconscious (like
  `Quit`). Saving does not break stealth.
- `apply_player_command` handles it exactly like the camp save: write to
  `save_path_for_hero(hero.name)` and log the outcome.
- The text frontend binds the key to `y` (unused in the dungeon) and documents
  it in the dungeon help as "y Save game". In town the key is inert (the camp
  save on key 2 is unchanged).

## Pregenerated Campaign Save

`gen_pregenerated` now seeds a Testing Tomb campaign save alongside the seven
procedural heroes. The shared `make_pregenerated_hero` helper was extracted from
`generate_and_save`, and the module save drives the real boot path:

- locate the shipped module via `find_module_dir(data, "testing_tomb")`;
- load its content, start a `GameSession` with a thief who can pick the
  entrance lock;
- play a few turns through `session.apply` (step to the corridor, pick the
  entrance door, walk to the tavern, hear the first rumor);
- `save_game` to `data/pregenerated_saves/Tomb Runner.dat`.

The template therefore boots into the tomb with the entrance door open, one
rumor heard, and the hero paused at the corridor tile — a non-trivial resume
through the same machinery a live campaign uses.

## Frontend Boot

The three load sites in `frontends/text/src/main.cpp` (`BX_LOAD_SAVE`, the saved
characters menu, and the legacy fallback) previously settled every dungeon save
at camp. They now call a shared `reenter_loaded_save` helper:

- a save with empty `module_id` is the procedural campaign unchanged: any
  dungeon state settles at camp and "Loaded <name>." is logged;
- a save with a `module_id` is a module campaign: `find_module_dir` +
  `load_module_content` locate and bind the content, `capture_campaign_state`
  takes the loaded Game, and `session.resume_module` rebuilds the map and
  restores heard rumors, door flags, position, and location. The hero stays
  in the module dungeon (no forced camp). A module save whose content cannot
  be found or bound is treated as an unusable load so the session does not
  silently degrade to the procedural campaign.

A live round-trip through the real binary (module template → `y` save → quit →
relaunch → still in the tomb at (2,3) with the door open) is the acceptance
proof.

## Tests

- `find_module_dir locates installed modules by declared module id` — finds the
  shipped `data/modules/testing-tomb`, and returns empty for unknown ids, empty
  ids, and a data dir with no modules root.
- Save format v7:
  - `Crafted v7 payload loads hero position from the format tail`.
  - `Save/load round-trips module campaign state, including hero position` —
    the position rides the binary round-trip with the module fields.
  - `Malformed saves: v7 files truncated in the position tail are rejected`
    without side effects.
  - `Old save versions still load with defaulted fields` — versions 1-5 default
    the position to {0, 0}; version 6 (which carries the module tail but not
    the position) is exercised separately against a v6 payload.
  - The bad-version list drops `7` (now legal) in favor of `8`: `{-1, 0, 8,
    1000000}`.
- `Dungeon Save command persists the game without advancing the turn` — saving
  works while paralyzed, leaves the monster untouched (no turn passed), and the
  written save loads back with hero and gold intact.
- `GameSession resume falls back to the entry tile for unusable positions` — a
  campaign whose recorded position sits on a wall tile (the pre-v7 {0, 0}
  default) resumes at the module entry instead of stranding the hero.
- `GameSession resume_module restores heard rumors and door flags` and `Module
  campaign state round-trips save/load and resumes` now also assert the hero
  resumes at the saved tile (2,3), not the entry.
- `Pregenerated Tomb Runner campaign save boots a module session` — loads the
  committed `data/pregenerated_saves/Tomb Runner.dat`, resolves the module
  through `find_module_dir`, and resumes it with the entrance door open at
  position (2,3) — the exact boot-path arc the frontend performs.

## Validation

- Debug and sanitizer builds: warning-free.
- Doctest: 205/205 cases, 15056/15056 assertions (Debug + sanitizer).
- ctest: PASS (Debug + sanitizer).
- Smoke (Debug): `printf 'x' | timeout 30 env BX_LOAD_SAVE=Hrothgar
  ./build/frontends/text/bx_crawl_text` still boots to town; `printf 'x' |
  timeout 30 env BX_LOAD_SAVE='Tomb Runner' ...` boots into the tomb dungeon
  (goblins, the open entrance door, and the hero at the corridor), both exit 0.
- Full frontend round-trip: boot the template, press `y` (dungeon save writes
  `~/.bx_crawl/saves/Tomb Runner.dat`), quit, relaunch from the user save — the
  hero resumes in the tomb at (2,3) with the entrance door open, exit 0.

TESTING TOMB is now a complete, playable module campaign: content, gameplay,
rumor delivery, and a campaign save/reload loop that starts, plays, saves,
quits, reloads, and resumes in-module — all data-driven, with no module-specific
engine rules.