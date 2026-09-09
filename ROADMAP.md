# Roadmap

This file tracks the next practical work for BX Crawl. The current priority is
finishing the architecture-review follow-up before expanding new gameplay.

## Current State

- Character generation, town, camp, dungeon entry/exit, save/load, shops, and
  equipment are playable.
- Dungeon exploration includes light, hunger, rations, traps, searching, door
  interaction, thief skills, cleric turning, spells, scrolls, throwing weapons,
  procedural sound effects, and secret doors.
- Combat mechanics include bear hug, stealthy-monster surprise modifiers,
  paralysis, normal bat swarm confusion, and giant vampire bat unconsciousness /
  blood drain.
- Monster playtest utilities build and smoke-test successfully:
  `list_monsters`, `show_monster`, `edit_monster`, `show_treasure_table`,
  `sim_surprise`, `show_map_features`, and `gen_pregenerated`.
- Architecture groundwork completed through Phase 16:
  - save/load hardening
  - `GameSession` application boundary
  - snapshot renderer separation
  - structured events and frontend sound consumer
  - stable `ContentId` and `RulesCatalog`
  - `CampaignState` model
  - module manifest/content loading
  - Testing Tomb entry module, map topology, runtime session entry, and door IDs
  - architecture decision records
  - Docker packaging fix
  - persistence extraction
  - combat helper and attack/morale extraction
  - spell execution extraction

## Architecture Review Track

Remaining architecture-review work should stay incremental and behavior-preserving
unless a phase explicitly says otherwise.

1. **Extract Persistence From `game.cpp`** — Done

   Save/load and save-path helpers now live in `engine/src/save.cpp`; the public
   API in `include/game.hpp` remains unchanged.

2. **Extract Combat From `game.cpp`** — Done

   Surprise, initiative, alive checks, peace announcement, attack resolution,
   morale, bear attacks, and fleeing combat helpers now live in
   `engine/src/combat.cpp`. `engine/src/game_internal.hpp` keeps the temporary
   private helper boundary out of the public API.

3. **Extract Spells From `game.cpp`** — Done

   Spell execution now lives in `engine/src/spells.cpp`. The current string-chain
   dispatch is preserved to keep the extraction behavior-preserving; a table can
   be introduced later if it reduces real coupling.

4. **Extract Exploration / Time / Traps**

   Move visibility, hunger/light timing, searching, trap handling, and dungeon
   movement helpers into `engine/src/exploration.cpp` behind existing tests.

5. **Extract Town / Inventory Flow**

   Move town state transitions, shops, equipment handling, and inventory helpers
   into `engine/src/town.cpp` / `engine/src/inventory.cpp`.

6. **Refactor Character Generation Boundary**

   Continue moving character generation toward pure generation/selection helpers,
   leaving stdin/stdout prompts in the text frontend.

7. **Introduce DiceRoller / RandomService**

   Wrap `Game::rng` usage for deterministic tests and future replay/roll-history
   support.

8. **Inventory Content IDs**

   Eventually replace fixed-count weapon/armor/gear arrays with ID-based
   inventory or validated content indexes.

## Testing Tomb Track

Testing Tomb remains the architectural proof that modules are data-only and do
not require module-specific C++ logic.

Already proven:

- manifest loading and validation
- stable module content IDs
- location and description loading
- map topology loading
- starting a `GameSession` at a module entry location
- stable module door object IDs

Deferred until after the near-term architecture refactors:

- runtime module door state and location transitions
- Room B
- encounter definitions and goblin instantiation from `bx.monster.goblin`
- treasure definitions and pickup state
- trap definitions and saving throw/damage integration
- tavern/module rumor tables and rumor delivery state
- module flags and campaign save/reload

## Gameplay Track

Gameplay work can continue after the current architecture cleanup or when a
module slice requires it.

High-value gameplay items still known:

- Goblins/orcs/kobolds: daylight penalties and leader morale effects where
  relevant.
- Shadows/wights/wraiths: drain effects and appropriate weapon immunity.
- Rust monsters/oozes/slimes: equipment destruction/corrosion consistency.
- Continue reviewing monster descriptions and stat data against the Basic Box PDF.
- Add Tavern rumors; the UI advertises them, but the Tavern command currently
  only implements paid rest.
- Replace stale `Monsters.md` with a generated reference or a pointer to the
  monster tools and data files.

## Recommended Next Task

Continue with **Extract Exploration / Time / Traps**.

Acceptance criteria:

- Visibility, hunger/light timing, searching, trap handling, and dungeon movement
  helpers move out of `engine/src/game.cpp` in behavior-preserving slices.
- Existing exploration and trap behavior tests pass unchanged.
- Public declarations in `include/game.hpp` remain compatible unless a dedicated
  exploration header is introduced for already-public functions.
- Full Debug and sanitizer validation pass.
