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
- Architecture groundwork completed through Phase 33:
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
  - undead drain and weapon immunity: wight/wraith touches drain one hero level
    (`apply_level_drain` mirrors `check_level_up`: hit die + CON off max/current
    HP, attack bonus recomputed for Fighters or -1 otherwise, spell slots
    re-granted at the drained level, XP dropped to the drained band's midpoint)
    and shadow touches drain 1 STR (via `apply_strength_drain`, bonuses
    follow); shadows/wights/wraiths carry a `WeaponImmunity` tier set at spawn
    (`SilverOrMagic` wights, `MagicOnly` shadows/wraiths) that `attack_internal`
    and `ranged_attack` honor through `hero_weapon_can_wound` (silver dagger is
    the game's only pass; spells, thrown holy water, and turning bypass melee)
  - rust monster corrosion: a rust monster touch (`corrodes_metal`, set at
    spawn) corrodes one piece of the hero's normal metal equipment per hit —
    worn chain/plate, then shield, then the wielded metal weapon (which leaves
    the hero unarmed at 1d2), then carried metal — via `apply_rust_corrosion`
    in the combat special chain; non-metal and silver gear (weapon material
    rules are classified in one place) are unharmed, a touch finds "no metal to
    corrode" otherwise, and destroyed equipment round-trips through save v7
    (ids, AC, and counts)
  - green slime contact: `apply_green_slime` (spawn flag in the combat special
    chain) dissolves the hero's worn non-magical gear — armor, shield, and
    wielded weapon, wood included — in one touch with a message naming what was
    eaten (silver survives), then clings (`green_slime_turns`): 1d8 each round
    the hero spends not scraping, via a new `Scrape` player command (frontend
    key `s`) that consumes a normal turn; gray ooze/ochre jelly already carry
    their correct B/X damage

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

- ~~Goblins/orcs: daylight to-hit penalty (-1 in hero light) and lair-leader
  morale effects.~~ Done: lair-leader morale in Phase 27, and the daylight
  penalty for goblins/orcs is implemented.
- ~~Shadows/wights/wraiths: drain effects and appropriate weapon immunity.~~
  Done in Phase 31: wight/wraith touches drain one hero level (hp/max-hp,
  attack bonus, spell slots, and XP dropped to the drained level's midpoint);
  shadow touches drain 1 STR; all three are immune to weapons without the
  right material (wights: silver or magic; shadows/wraiths: magic only —
  spells, thrown holy water, and turning bypass exactly as before).
- ~~Rust monster: metal corrosion consistency.~~ Done in Phase 32: a rust
  monster touch (`corrodes_metal`, set at spawn) corrodes one normal-metal
  piece per hit — worn chain/plate, then shield, then the wielded metal weapon
  (leaving the hero unarmed at 1d2), then a carried metal item — with a message
  and the equipment actually removed; silver, non-metal, and the wooden missile
  weapons are unharmed (weapon-material rules live in one place), and a touch
  with nothing to eat finds "no metal to corrode".
- ~~Green slime: contact dissolution and 1d8 clinging.~~ Done in Phase 33:
  `apply_green_slime` (spawn flag in the combat special chain) dissolves the
  worn non-magical armor, shield, and wielded weapon — wood included — in one
  touch, then clings for 1d8 each round until a `Scrape` action (new key `s`)
  consumes a turn to remove it; silver survives, gray ooze/ochre jelly already
  deal their correct B/X damage (B/X has no "ignores armor" damage rule — AC
  only affects to-hit).
- Slime counter-attacks: done in Phase 34 for current engine/data support
  (green slime fire-only, burning oil / Fire Ball as fire, ochre jelly fire
  splitting). Lightning Bolt now provides a lightning damage source and triggers
  the same ochre jelly split rule; black pudding remains blocked on missing
  monster data.
- Continue reviewing monster descriptions and stat data against the Basic Box PDF.
- Replace stale `Monsters.md` with a generated reference or a pointer to the
  monster tools and data files.

## Recently Completed

~~**Slime damage immunities and splitting**~~ — done in Phase 34 for the rules
that have current engine/data support. Green slime is now fire-only (`FireOnly`:
melee/thrown weapons pass through), burning oil is a new fire source, Fire Ball
shares the same fire-damage path, and base ochre jellies split into two smaller
`Ochre Jelly'` offspring when fire would slay them. Lightning Bolt now provides a
lightning damage source and also triggers ochre jelly splitting. The original
black pudding wording remains future work because there is no Black Pudding
monster row yet.

Acceptance criteria completed:

- A green slime is unharmed by any melee/thrown weapon (a "passes right
  through!" message), but suffers damage from burning oil and Fire Ball; it
  stays correct against the existing weapon-immunity tests.
- A base ochre jelly reduced by fire splits into two smaller `Ochre Jelly'`
  monsters instead of dying outright, with no crash and a dungeon message.
- Existing weapon/armor, combat, and save/load tests stay green; Debug +
  sanitizer validation pass.

Remaining backlog:

- Add or generate a Black Pudding monster row before implementing black-pudding
  splitting.

(The Testing Tomb module proof is complete: content, gameplay, rumor delivery,
and a full campaign save/reload loop. Remaining Gameplay Track items above are
the backlog.)

## Recommended Next Task

**Monster description/stat review** — continue checking monster descriptions and
stat data against the Basic Box PDF, then replace stale `Monsters.md` with a
generated reference or a pointer to the monster tools and data files.
