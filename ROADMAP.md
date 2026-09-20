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
- Architecture groundwork completed through Phase 30:
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
  - exploration, time, and trap handling extraction
  - town flow and inventory/equipment helper extraction
  - character generation boundary (pure rules in the engine, prompts in the text
    frontend)
  - DiceRoller / RandomService (`Game::rng` wrapped behind a dedicated dice rule
    API)
  - inventory content IDs (validated item catalog + ID-based inventory access)
  - monster and spell content lookups (`bx::monster_index` / `bx::spell_index`
    from the catalog; catalog-backed spell level/class rules)
  - module scripted encounters: `encounters.json` binds tile positions to
    monster content ids; `GameSession::start_module` instantiates them through
    the rules catalog
  - module door state and location transitions: `start_module` plants the
    declared `locked` state on the runtime door tile and the core open/close
    commands drive it unchanged; crossing the door routes the session's
    `current_location_id` between the declared `from`/`to` locations
  - Room B envelope: the tomb map grows to 10x5 with a second locked door and
    the Room B location, skeleton guard, gold pile, and pit — all resolved by
    the same loader, seeding, and door-route machinery with no new engine
    logic
  - module rumor tables and tavern delivery state: `rumors.json` declares
    rumor content ids bound to module text blocks; the tavern `Listen` command
    serves each rumor once and tracks the heard ids as session state
  - module flags and campaign save/reload: `Game` carries the module identity
    and per-door flags, `CampaignState` captures them, the binary save format
    advances to v6 with the module tail, and `GameSession::resume_module`
    re-binds a module from content plus captured state so heard rumors stay
    heard and picked doors stay open
  - module campaign boot: `find_module_dir` locates the shipped module under
    `data/modules`, `gen_pregenerated` seeds a Testing Tomb campaign save
    through the real boot path (`start_module` → play → save v7 with hero
    position), the frontend transparently re-enters a module save through
    `resume_module` instead of settling at camp, and a dungeon `Save` command
    (text key `y`) lets a hero play, save, quit, reload, and resume in-module

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

4. **Extract Exploration / Time / Traps** — Done

   Visibility, hunger/light timing, walking-monster timing, trap triggering,
   searching, and hero dungeon movement now live in `engine/src/exploration.cpp`.
   Public declarations in `include/game.hpp` remain unchanged.

5. **Extract Town / Inventory Flow** — Done

   Town/menu state transitions and shop flows now live in
   `engine/src/town.cpp`; weapon/armor/gear rules and equipment helpers now live
   in `engine/src/inventory.cpp`. Public declarations in `include/game.hpp`
   remain unchanged.

6. **Refactor Character Generation Boundary** — Done

   Level-1 character generation is now a set of non-interactive rule helpers in
   `include/chargen.hpp` / `engine/src/chargen.cpp`, and the text menus and
   prompts live in `frontends/text/src/char_session.cpp`. The engine no longer
   has an interactive init entry; new games start with a hero built by the
   frontend.

7. **Introduce DiceRoller / RandomService** — Done

   `DiceRoller` (`include/dice.hpp` / `engine/src/dice.cpp`) now owns `Game::rng`
   and every random draw funnels through its `d()` / `range()` rule calls
   (`roll_d` and the map generator's `rand_range` are thin wrappers). It keeps
   the `std::mt19937` surface (seed / operator() / result_type), so existing
   seeding, snapshot, and save code is unchanged. This is the single hook point
   for future replay / roll-history support.

8. **Inventory Content IDs** — Done

   `bx::weapon_index` / `armor_index` / `gear_index` / `inventory_item_by_id`
   (`include/item_catalog.hpp` / `engine/src/item_catalog.cpp`) are the single
   validated mapping from stable item content ids to the `bx_data` table
   positions, pinned against `Entity`'s count arrays by `static_assert`. New
   `include/inventory.hpp` adds ID-based access (`item_count`, `give_item`,
   `set_item_count`, `consume_owned_item`) for all three tables. Kit-writing
   call sites (chargen shield buy, torch lighting, gear shop purchase, save
   migration, pregen generator) no longer use magic array positions.

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
- encounter definitions and goblin instantiation from `bx.monster.goblin`
  (`encounters.json` names monsters by stable content id; the engine resolves
  them through the rules catalog and spawns them at module start)
- treasure definitions and pickup state (`treasures.json` places gold/gem piles
  by stable content id at fixed tiles; `GameSession` seeds the runtime map and
  the core Get command resolves pickup, coins, XP, sound, and logging
  unchanged)
- trap definitions and trigger state (`traps.json` names pit/dart/poison_dart
  traps by stable content id at fixed tiles; `GameSession` plants the tile
  trap state and the core move/search rules resolve trigger, saving throw,
  damage, and disarming unchanged)
- door state and location transitions (the declared locked door is planted on
  the runtime door tile at module start; the core open/close commands drive
  pick/force/close behavior unchanged, and crossing the door tile moves
  `current_location_id` between the declared endpoint locations)
- Room B (a second tomb envelope reached through a second locked door; the
  Room B location, skeleton guard, gold pile, and pit are resolved by the
  same loader, seeding, and door-route machinery with no new engine logic)
- tavern/module rumor tables and rumor delivery state (`rumors.json` declares
  rumor content ids bound to module text blocks; `GameSession::start_module`
  binds the resolved texts to `Game`, the tavern `Listen` command (tavern key
  2, replacing the old `Help` stub) serves each rumor exactly once, and the
  heard rumor ids are tracked as session state per campaign)
- module flags and campaign save/reload (`Game` carries module identity and
  mutable door flags, `CampaignState` captures/restores them, save format v6
  persists the module tail, and `GameSession::resume_module` restores a
  replayed hero's heard rumors, open doors, and location from content plus
  captured state)
- module campaign boot (a Testing Tomb campaign save is seeded by
  `gen_pregenerated` through the real boot path, the frontend re-enters a
  module save through `resume_module` instead of settling at camp, the save
  format advances to v7 with the hero position so a campaign resumes in-place,
  and a dungeon `Save` command — text key `y` — supports play → save → quit →
  reload → resume in-module with heard rumors, open doors, and position intact)

Deferred:

- (nothing currently deferred; the Testing Tomb module proof is complete:
  content, gameplay, rumor delivery, and a campaign save/reload loop)

## Gameplay Track

Gameplay work can continue after the current architecture cleanup or when a
module slice requires it.

High-value gameplay items still known:

- Goblins/orcs/kobolds: daylight penalties and leader morale effects where
  relevant.
- Shadows/wights/wraiths: drain effects and appropriate weapon immunity.
- Rust monsters/oozes/slimes: equipment destruction/corrosion consistency.
- Continue reviewing monster descriptions and stat data against the Basic Box PDF.
- Replace stale `Monsters.md` with a generated reference or a pointer to the
  monster tools and data files.

## Recommended Next Task

**Undead drain and weapon immunity** — bring the remaining core B/X monster
trait rules inline with the Basic Box. Shadows, wights, and wraiths currently
use generic melee like any monster: B/X specifies that their hit drains a
character level (with the corresponding hit die, attack bonus, and class
abilities dropped), and that they are immune to non-magical, non-silver
weapons. Add the two rules to the combat path (attack resolution + the
post-hit level-drain) driven by monster data where it lives today, with the
usual Debug + sanitizer + smoke validation.

Acceptance criteria:

- A shadow/wight/wraith hit drains one hero level and its effects (hp/max-hp,
  attack bonus, spells, class minimums) actually come off the hero, with a
  dungeon message and no crash at zero hit dice.
- Their weapon immunity is honored: non-magic, non-silver melee/thrown weapons
  do not wound them; silver or magic weapons (and the two cases that already
  bypass such rules, like undead turning) still work.
- Existing monster/combat tests stay green; Debug + sanitizer validation pass.

(The Testing Tomb module proof is complete: content, gameplay, rumor delivery,
and a full campaign save/reload loop. Gameplay Track items above remain the
backlog.)
