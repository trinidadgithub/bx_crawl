# BX Crawl Architecture Review — Follow-Up

## Executive Summary

The original review is broadly correct: BX Crawl is a healthy, test-covered prototype that should evolve by incremental extraction rather than by rewrite. The current implementation is valuable precisely because it has working behavior, an explicit `Game` state object, deterministic `std::mt19937`-based tests, a separate `bx_engine` target, and a text frontend executable.

The second-pass conclusion is more specific:

- Keep `Game` as the internal authoritative mutable state for now.
- Add a stable external boundary around it before modules or alternate frontends depend on internals.
- Treat future adventure modules as data packages that reference rules content through stable content IDs.
- Separate rules content, adventure content, and campaign/runtime state explicitly.
- Start with a tiny data-only proof-of-concept module, `testing-tomb`, before designing a broad module standard.
- Defer scripting, plugin systems, ECS, SQLite, and complex service frameworks until actual requirements justify them.

The most important invariant is valid and should guide the architecture:

```text
Modules describe the adventure.
The BX rules package describes rules content.
The engine executes the rules.
Campaign state records what happened.
The frontend presents the result.
```

The current code violates this invariant only because it predates the module concept: procedural dungeon generation, presentation strings, terminal rendering, sound calls, and persistence are mixed into the same engine implementation. Those are fixable by extraction behind existing tests.

## Validation of Original Review

Confirmed findings:

- `Game` is the authoritative internal state. `include/game.hpp` stores map, hero, monsters, RNG, descriptions, event log, progression, dungeon level, timers, rooms, and town state directly in `struct Game`.
- `Entity` is a broad mixed-purpose record. `include/entity.hpp` contains PC stats, monster stats, spell slots, equipment, supplies, AI state, conditions, light state, and monster traits.
- `PlayerCommand` is a useful but early-stage command abstraction. `include/game.hpp` exposes `PlayerCommandType` and `PlayerCommand`, and tests already drive behavior through `apply_player_command` / `apply_town_command`.
- `game.cpp` is the central architectural pressure point. It contains combat, surprise, XP, monster spawning, morale, spells, rendering, AI, town flow, persistence, and save helpers.
- Rendering is inside the engine. `render()` and `render_town_screen()` in `engine/src/game.cpp` write directly to `std::cout` and use ANSI escape sequences.
- Character generation is frontend-coupled. `engine/src/chargen.cpp` performs rule work and interactive `std::cin` / `std::cout` prompting in the same function.
- Persistence is binary file serialization in `engine/src/game.cpp`, not a database. It currently persists mostly hero/meta state, not full dungeon or module state.
- Sound is directly invoked from rule execution. Combat, movement, traps, and spells call `sound_*` functions that eventually fork/exec `aplay`/`paplay` in `engine/src/sound.cpp`.
- Randomness is mostly deterministic and testable through `Game::rng` and `roll_d(Game&, int)`.
- `bx_data.hpp` is a useful rules-data package but lacks stable namespaced IDs suitable for independent modules.
- Existing tests are a major asset. `tests/test_core.cpp` covers many rule and state-transition behaviors with seeded games.

Findings requiring modification:

- `PlayerCommand` should not simply be renamed `GameCommand` as-is. The current `spell_index` field is reused as a generic selection index for town/shop/equipment flows, so the first API step should either preserve `PlayerCommand` internally or introduce a clearer wrapper while keeping the old type for tests.
- The text frontend is not purely presentation. `frontends/text/src/main.cpp` owns parts of turn sequencing: visibility/render timing, initiative rolling, monster turns, death flow, and peace announcement checks. A future `GameSession` should absorb this sequencing before a GUI can reuse the engine cleanly.
- `ContentCatalog` should be split conceptually. One flat catalog is useful internally, but architecture should explicitly distinguish rules content, adventure module content, and campaign/runtime state.
- `GameSession` should initially wrap `Game`, not replace it. Moving directly to a private, fully encapsulated domain model would create unnecessary churn.

Findings disagreed with:

- No major disagreement. The original review's direction is sound.

Important issues missed or underemphasized:

- Procedural dungeon generation in `engine/src/map.cpp` currently owns doors, traps, stairs, room treasure, and map topology. Module support needs an alternate map/location source without deleting this procedural generator.
- `frontends/text/src/main.cpp` reads `g.hero.spells`, `g.state`, and `g.town_submenu` to decide prompts and command mapping. A frontend-neutral snapshot must expose enough choice/menu state to avoid direct `Game` reads.
- The public API currently exposes persistence helpers and raw save paths in `game.hpp`. Future frontends can depend on these accidentally unless a `SaveService`/application boundary is introduced.
- Environment-variable controls (`BX_NO_MONSTERS`, `BX_TEST_MONSTER`, `BX_LOAD_SAVE`) are practical developer tools, but they are hidden dependencies. They should eventually move to explicit session/test configuration.

## Architectural Principles

Preserve:

- Explicit authoritative state.
- Deterministic RNG.
- Value semantics.
- Behavior tests.
- `PlayerCommand`-style command input.
- `bx_engine` as a separate library.
- Data-driven BX rules tables.
- Text frontend as a separate executable.

Prefer:

- Free/stateless rule functions before service classes.
- Composition before inheritance.
- Small DTOs over broad public mutable state for external APIs.
- Stable textual IDs in files, lightweight wrappers in C++ only where useful.
- Data-only modules first.
- Declarative module triggers before scripting.

Avoid:

- Rewriting `Game` immediately.
- Adding interface hierarchies without a concrete consumer.
- Making all domain fields private in one churn-heavy pass.
- Letting modules or new frontends depend on raw `Game`, `Entity`, `Tile`, array indexes, or save layouts.

## Target Component Model

Conceptual target:

```text
Rules Content Package
    |
    v
Rules Engine
    |
    v
Adventure Module Content
    |
    v
Campaign / Runtime State
```

Presentation target:

```text
Text UI | GUI | TUI | Web UI
        |
        v
Application / GameSession API
        |
        v
BX Engine
        |
        +-- Rules Content Catalog
        +-- Module Definition
        +-- Campaign State
```

Practical component model for this codebase:

```text
+-----------------------------------------------------+
| Frontends                                           |
| text/main.cpp today, GUI/TUI/Web later              |
+--------------------------+--------------------------+
                           |
                           v
+-----------------------------------------------------+
| Application Boundary                                |
| GameSession | GameCommand | ActionResult | Snapshot |
+--------------------------+--------------------------+
                           |
                           v
+-----------------------------------------------------+
| bx_engine                                            |
| current Game + extracted combat/spells/exploration  |
+-------------+-------------------+-------------------+
              |                   |
              v                   v
+-------------------------+   +-----------------------+
| Domain / Runtime State  |   | Content Catalog        |
| Game, Entity, Tile      |   | rules + module defs    |
+-------------------------+   +-----------------------+
              |                   ^
              v                   |
+-------------------------+   +-----------------------+
| Persistence             |   | Module Loader          |
| campaign/save state     |   | validates module data  |
+-------------------------+   +-----------------------+
```

This fits the existing project because it wraps current `Game` and existing procedural logic first, then permits gradual extraction.

## Rules / Adventure / Campaign Separation

BX Crawl should explicitly separate three categories.

Rules content describes what things are:

- Classes.
- Spells.
- Equipment.
- Weapons.
- Armor.
- Monsters.
- Saving throws.
- Advancement tables.
- Combat tables.
- Standard treasure definitions.
- Standard conditions and traits.

Current location: mostly `include/bx_data.hpp`, generated from `dnd-data` JSON/schema sources.

Adventure module content describes where and how rules content appears:

- Locations.
- Maps.
- Rooms.
- Monster placements.
- NPC instances.
- Treasure placements.
- Factions.
- Traps.
- Encounters.
- Rumors.
- Quests.
- Module events.
- Wandering monster tables.
- Module-specific descriptions.

Current location: not represented yet, except procedurally inside `map.cpp` and spawning logic in `game.cpp`.

Campaign/runtime state describes what has happened:

- Player character state.
- Current module and location.
- Current time.
- Monster instance state.
- Defeated encounters.
- Opened doors.
- Triggered traps.
- Discovered locations.
- Inventory.
- World/module flags.
- Quest state.
- RNG state or seed when deterministic continuation is required.

Current location: partially in `Game`, partially in save files, but not fully persisted. Since Phase 29 the module rows above are concrete: `Game` carries module identity, `heard_rumors`/`module_door_states`/`current_location_id` capture opened doors and world/module flags, and `CampaignState` + save format v6 persist them (restored via `GameSession::resume_module`). Phase 30 adds the hero position to that caption: save format v7 writes `pos` after the v6 door list (realized by backend loads in Phases 13-16), `resume_module` guards it (in-bounds, not a wall/closed door; otherwise falls back to the module entry tile), a dungeon `Save` command (non-turn, like `Quit`) writes `save_path_for_hero`, and `find_module_dir` locates shipped module content by scanned manifest id under `data/modules` so a module save re-binds through the same path the engine test uses.

Recommended C++ boundaries:

```cpp
struct RulesCatalog;      // immutable core BX content
struct ModuleDefinition;  // immutable adventure content
struct CampaignState;     // persistent mutable state
struct Game;              // current runtime state, can initially remain authoritative
```

Do not implement all four at once. Introduce `RulesCatalog` / `ModuleDefinition` only when the first module loader begins. `CampaignState` can initially be a save-specific DTO populated from `Game`.

## Engine API Boundary

The proposed `GameSession` boundary fits the existing code if introduced as a wrapper, not a rewrite.

Minimum viable shape:

```cpp
enum class GameCommandType {
    None,
    Move,
    Wait,
    Quit,
    Open,
    Close,
    Rest,
    ThrowKnife,
    ThrowDart,
    ThrowHolyWater,
    Cast,
    Turn,
    Inventory,
    Help,
    Get,
    Use,
    Hide,
    Search
};

struct GameCommand {
    GameCommandType type{GameCommandType::None};
    int dx{0};
    int dy{0};
    int selection{-1};
};

struct ActionResult {
    bool keep_playing{true};
    std::vector<GameEvent> events;
};

class GameSession {
public:
    ActionResult apply(const GameCommand& command);
    GameStateSnapshot snapshot() const;

private:
    Game game_;
};
```

Initial implementation can map `GameCommand` to current `PlayerCommand`, call `apply_player_command` or `apply_town_command`, then derive events from newly appended `Game::event_log` entries.

Minimum snapshot fields the current text frontend needs:

```cpp
struct GameStateSnapshot {
    GameState state;
    int town_submenu;
    int width;
    int height;
    std::vector<TileView> tiles;
    std::vector<EntityView> entities;
    HeroView hero;
    std::vector<std::string> log_lines;
    std::vector<MenuChoice> choices;
};
```

`Prompt` is not needed for the first dungeon command boundary if snapshots expose choices. It becomes useful for character generation and multi-step commands:

```cpp
struct Prompt {
    std::string prompt_id;
    std::string text;
    std::vector<MenuChoice> choices;
};
```

Important sequencing change: `GameSession::apply` should eventually own the current loop logic in `frontends/text/src/main.cpp`: visibility update, initiative, monster turns, player command, death handling, and peace announcement.

## Stable Content ID Strategy

External files should use stable textual IDs. Internal C++ can start with strings and later wrap them.

Recommended ID format:

```text
bx.monster.goblin
bx.monster.ghoul
bx.item.long_sword
bx.item.chain_mail
bx.spell.sleep
bx.spell.magic_missile
bx.class.fighter
module.testing_tomb.location.entrance
module.testing_tomb.room.room_a
module.testing_tomb.encounter.room_b_goblins
```

Rules:

- Lowercase ASCII.
- Dot-separated namespace segments.
- First segment is either `bx` or `module`.
- `bx.*` is reserved for core B/X rules content.
- `module.<module_id>.*` belongs to one module.
- Module IDs use lowercase letters, digits, and underscores.
- IDs are unique within a loaded content catalog.
- Save files store textual IDs, not array indexes.
- Module files reference textual IDs.

Validation rules:

- Reject empty IDs.
- Reject unknown namespaces.
- Reject duplicate IDs at load time.
- Reject module content that references missing rules/adventure IDs.
- Reject module IDs that attempt to define `bx.*` content.

C++ representation tradeoff:

```cpp
struct ContentId {
    std::string value;
};
```

This wrapper is useful because it makes APIs explicit without a heavy type system. Avoid separate `MonsterId`, `ItemId`, `SpellId`, etc. until misuse becomes common. Module files should remain textual JSON IDs.

Lookup strategy:

```cpp
const MonsterDef* RulesCatalog::find_monster(ContentId id) const;
const ItemDef* RulesCatalog::find_item(ContentId id) const;
const SpellDef* RulesCatalog::find_spell(ContentId id) const;
const RoomDef* ModuleDefinition::find_room(ContentId id) const;
```

For initial migration, map existing `bx_data` short IDs/names to canonical IDs in a small adapter table. Do not rewrite `bx_data.hpp` first.

## Rules Content Architecture

Rules content should remain mostly data where it already is data:

- Monster AC, HD, morale, damage strings, XP values.
- Weapon damage and price.
- Armor AC and price.
- Gear IDs, prices, weights.
- Saving throw tables.
- Spell levels and class membership.
- Advancement tables.
- Treasure tables.

Rules behavior should remain C++ code:

- Attack resolution.
- Saving throw resolution.
- Turn/initiative sequence.
- Movement and blocking rules.
- Encumbrance calculations.
- Spell execution.
- Morale checks.
- Search/listen/trap resolution.

Do not make all behavior data-driven. That would turn JSON into a homemade scripting language.

Good places for data-driven behavior:

- Monster traits.
- Spell metadata and targeting requirements.
- Module triggers that call existing engine operations.
- Trap definitions that call existing saving throw/damage operations.

Poor places for data-driven behavior right now:

- Full combat math.
- Spell implementations with complex state changes.
- Monster AI behavior trees.
- Arbitrary conditional procedural logic.

## Adventure Module Architecture

Start with data-only modules.

Minimum viable module structure:

```text
modules/
  testing-tomb/
    module.json
    locations.json
    maps/
      tomb.json
    encounters.json
    treasures.json
    traps.json
    text/
      descriptions.txt
```

Minimum manifest:

```json
{
  "module_id": "testing_tomb",
  "title": "Testing Tomb",
  "version": "0.1.0",
  "author": "BX Crawl",
  "ruleset": "bx.moldvay_basic",
  "engine_min_version": "0.1.0",
  "content_schema_version": 1,
  "entry_location_id": "module.testing_tomb.location.entrance",
  "files": {
    "locations": "locations.json",
    "map": "maps/tomb.json",
    "encounters": "encounters.json",
    "treasures": "treasures.json",
    "traps": "traps.json",
    "descriptions": "text/descriptions.txt"
  }
}
```

Modules should reference rules content instead of duplicating it:

```json
{
  "encounter_id": "module.testing_tomb.encounter.room_b_goblins",
  "monsters": [
    { "monster_id": "bx.monster.goblin", "count": "2" }
  ]
}
```

Module-specific content can extend the content catalog inside the module namespace:

```text
module.testing_tomb.monster.goblin_captain
module.testing_tomb.item.cursed_silver_key
module.testing_tomb.trap.false_floor
```

Module-specific definitions should be explicit variants or additions:

```json
{
  "monster_id": "module.testing_tomb.monster.goblin_captain",
  "base_monster_id": "bx.monster.goblin",
  "name": "Goblin Captain",
  "overrides": {
    "hp": 8,
    "morale": 9,
    "damage_bonus": 1
  }
}
```

This avoids duplicating every goblin stat while allowing unique adventure content.

## Campaign State Architecture

Future campaign persistence must store what changed, not duplicate immutable module content.

Minimum campaign state to resume a module accurately:

- Engine save format version.
- Rules version.
- Module ID and module version.
- Current location/room ID.
- Current dungeon/map ID.
- Game time and turn counters.
- RNG state or replay seed/roll count.
- Hero/party state.
- Inventory and equipment by content ID.
- Spell slots and active conditions.
- Door states by module object ID.
- Trap states by module object ID.
- Treasure states by module object ID.
- Monster instance state by runtime instance ID and source content ID.
- Discovered/explored map state.
- Module flags and quest state.

Reconstruct from immutable module content:

- Room descriptions.
- Base map topology.
- Base encounter definitions.
- Base treasure definitions.
- Base monster/item/spell definitions.
- Static faction/NPC definitions.

Runtime monster instance example:

```json
{
  "entity_instance_id": "run.monster.0004",
  "source_id": "bx.monster.goblin",
  "location_id": "module.testing_tomb.room.room_b",
  "hp": 3,
  "alive": true,
  "position": { "x": 12, "y": 4 },
  "conditions": []
}
```

The current binary save can be hardened first. A later campaign save can be JSON or structured binary. JSON is better for early module work because it is inspectable and easier to validate. SQLite should wait until there is a demonstrated need for query-heavy campaign data.

## Frontend Independence

Current GUI blockers:

- `render()` writes maps, HUD, menus, and logs directly to `std::cout`.
- `render_town_screen()` embeds terminal-specific layout and ANSI clear-screen codes.
- `chargen.cpp` blocks on stdin/stdout.
- `frontends/text/src/main.cpp` reads `Game` internals to display spell choices and menus.
- Sound is triggered inside rules logic instead of from frontend/application events.
- Event messages are plain text rather than structured facts.
- Turn sequencing is split between the frontend loop and engine calls.

Minimum changes to enable a future GUI without duplicating rules:

1. Add `GameStateSnapshot` generation from `Game`.
2. Move `render()` into the text frontend and have it render snapshots.
3. Add `GameSession::apply` to own one complete command turn.
4. Add structured events while preserving text log lines.
5. Move sound dispatch out of rule functions and into an event consumer.
6. Split character generation into a pure generator plus frontend prompt flow.

The first GUI should consume the same `GameSession` API as the text frontend.

## Structured Event Architecture

Structured events are justified, but an event bus is not yet justified.

Start with a simple return vector:

```cpp
enum class GameEventType {
    LogMessage,
    AttackRolled,
    AttackHit,
    AttackMissed,
    DamageApplied,
    EntityKilled,
    SavingThrowRolled,
    DoorOpened,
    DoorClosed,
    TrapTriggered,
    TreasureFound,
    SpellCast,
    TimeAdvanced,
    LocationEntered,
    EncounterStarted
};

struct GameEvent {
    GameEventType type;
    std::string text;
    ContentId actor_id;
    ContentId target_id;
    int amount{};
};
```

Do not create dozens of event types first. Initial events should cover the behavior that current frontends/sound/tests need:

- `LogMessage` for compatibility.
- `AttackHit` / `AttackMissed`.
- `DamageApplied`.
- `EntityKilled`.
- `DoorOpened` / `DoorClosed`.
- `TrapTriggered`.
- `TreasureFound`.
- `SpellCast`.

Migration path:

1. Keep `Game::event_log` unchanged.
2. Add `Game::events` or have `GameSession` capture new log lines as `LogMessage` events.
3. Gradually replace direct `log_event` calls with helpers that append both structured events and text.
4. Tests can migrate from string matching to event assertions incrementally.

## Persistence Strategy

Validate and harden the current save system before replacing it.

Immediate save hardening:

- Validate string lengths before allocation.
- Check every read/write operation.
- Reject invalid enum values.
- Reject impossible numeric ranges where practical.
- Load into a temporary `Game` and assign only after success.
- Sanitize hero names used as filenames.
- Use `std::error_code` overloads for filesystem enumeration where failure should not throw.

Eventual persistence boundary:

```text
Rules Content       immutable, loaded from engine/rules package
Module Content      immutable, loaded from module directory
Campaign State      persistent mutable save data
Runtime State       in-memory Game/GameSession state
User Configuration  frontend/application owned
```

Format recommendation:

- Keep current binary save until campaign/module persistence begins.
- For first module campaign saves, prefer JSON with strict validation and version fields.
- Revisit structured binary once the save model stabilizes.
- Avoid SQLite until there is evidence that querying/indexing state is a real problem.

## Testing Tomb Proof of Concept

`testing-tomb` should be deliberately tiny and architectural, not a real adventure.

Layout:

```text
Entrance
    |
    v
Room A
    |
    +-- locked door
            |
            v
         Room B
            |
+-- 2 goblins
    +-- trapped chest
    +-- treasure
```

Status: the proof of concept is complete (Phase 30). Manifest/schema/content
loading, map/location/description loading, doors with runtime state, scripted
encounters, treasure and trap seeding, module flags, state transitions, and
save/reload with state restoration all work data-only with zero module-specific
C++ engine logic, and the campaign now boots end-to-end: `gen_pregenerated`
seeds `data/pregenerated_saves/Tomb Runner.dat` through the real boot path,
and the frontend re-enters it (`resume_module`) so a hero can play, save
(dungeon `Save`, text key `y`), quit, reload, and resume in-module with heard
rumors, open doors, and position intact.

What it must prove:

- Manifest loading.
- Schema validation.
- Stable content ID lookup.
- Map/location loading.
- Room descriptions.
- Doors.
- Encounter definitions.
- Monster instantiation using `bx.monster.goblin`.
- Combat using existing engine rules.
- Treasure using existing item/coin rules.
- Trap triggering using existing trap/saving throw logic.
- Module flags.
- State transitions.
- Save/reload/state restoration.

Critical invariant:

```text
Testing Tomb must require no module-specific C++ logic.
```

If special C++ is required, the missing concept should be added to the module contract only if it is generally useful.

Example room definition:

```json
{
  "room_id": "module.testing_tomb.room.room_b",
  "description_id": "module.testing_tomb.text.room_b",
  "encounter_id": "module.testing_tomb.encounter.room_b_goblins",
  "features": [
    {
      "feature_id": "module.testing_tomb.feature.trapped_chest",
      "type": "chest",
      "trap_id": "module.testing_tomb.trap.poison_needle",
      "treasure_id": "module.testing_tomb.treasure.chest_gold"
    }
  ]
}
```

## Testing Strategy

Architecture tests should exercise public state and events, not terminal output.

First module test shape:

```cpp
auto module = loader.load("modules/testing-tomb");
REQUIRE(module);

GameSession session{*module};
auto result = session.apply(GameCommand::enter_location(
    ContentId{"module.testing_tomb.location.entrance"}));

auto snapshot = session.snapshot();
CHECK(snapshot.location_id == ContentId{"module.testing_tomb.location.entrance"});
```

Later end-to-end test:

```text
load module
start game
enter room A
open locked door
enter room B
spawn goblin encounter
fight goblins
open chest
trigger trap
collect treasure
save
destroy session
reload
verify relevant state is identical
```

Continue preserving current tests. Add new architecture tests alongside them. Do not refactor all tests at once.

## Architecture Decision Records

Implemented in `docs/adr/`:

ADR: Engine / Frontend Boundary (`docs/adr/0001-engine-frontend-boundary.md`)

```text
Decision: Frontends interact with gameplay through commands, action results/events,
and snapshots rather than raw Game/Entity/Tile internals.

Rationale: Enables text, GUI, TUI, and tests to share one rules implementation.
```

ADR: Data-First Modules (`docs/adr/0002-data-first-modules.md`)

```text
Decision: Adventure modules are declarative data packages and do not initially
contain executable code.

Rationale: Avoids plugin ABI, scripting sandbox, and arbitrary-code security risks.
```

ADR: Stable Content IDs (`docs/adr/0003-stable-content-ids.md`)

```text
Decision: Rules and module content reference each other through stable textual IDs,
not array positions or implementation-specific indexes.

Rationale: Saves and modules must survive code/data reordering.
```

ADR: Rules vs Adventure vs Campaign State (`docs/adr/0004-rules-adventure-campaign-state.md`)

```text
Decision: Immutable rules content, immutable adventure definitions, and mutable
campaign state are architecturally distinct.

Rationale: The engine should execute content without hard-coding adventures, and
saves should record changes rather than duplicate definitions.
```

Do not create more ADRs until real decisions arise.

## Incremental Refactoring Plan

Extraction order should follow responsibility boundaries, not file size alone.

1. Safety first: save/load hardening.
2. Introduce `GameSession` as a wrapper around existing `Game`.
3. Move command-turn sequencing from `frontends/text/main.cpp` into `GameSession::apply`.
4. Add snapshots and move rendering to the text frontend.
5. Add simple structured events and keep `event_log` compatibility.
6. Move sound calls to an event consumer.
7. ~~Extract persistence into `save.cpp` / `persistence.cpp`.~~ Done in `engine/src/save.cpp`.
8. Extract combat into `combat.cpp` with free functions operating on `Game`. Started with surprise, initiative, alive checks, and peace announcement.
9. Extract spell execution into `spells.cpp` and add a small spell dispatch table.
10. Extract exploration/search/traps/time into `exploration.cpp`.
11. Extract town/shop/equipment flows into `town.cpp` / `inventory.cpp`.
12. Refactor character generation into pure generation functions plus frontend prompts.

Function style recommendation:

- Keep free functions while they operate directly on `Game`.
- Add classes only when state/ownership requires it, e.g. `GameSession`, `ModuleLoader`, `RulesCatalog`.
- Avoid introducing interfaces for single implementations.

## Dependency-Aware Implementation Roadmap

Dependency order:

```text
Save robustness
    |
    v
GameSession wrapper
    |
    v
Turn sequencing in engine boundary
    |
    v
Snapshots + renderer extraction
    |
    v
Structured events + sound as consumer
    |
    v
Content IDs + rules catalog adapter
    |
    v
Campaign state model
    |
    v
Module manifest loader
    |
    v
Testing Tomb
    |
    v
Declarative triggers
```

Work that can happen in parallel:

- Save hardening and Docker/package cleanup. Docker/package cleanup is done.
- Documentation ADRs and content ID format design.
- Spell dispatch design and monster trait schema design.
- Snapshot DTO design and renderer extraction preparation.

Work that should wait:

- Module loader should wait until stable content IDs are defined.
- Full campaign persistence should wait until module object IDs are defined.
- Declarative triggers should wait until `testing-tomb` exposes concrete needs.

## Things We Should Explicitly NOT Build Yet

Do not build these now:

- Entity Component System.
- C++ plugin ABI.
- Shared-library modules.
- Lua or JavaScript scripting.
- SQLite persistence.
- Networking or multiplayer.
- Dependency-injection framework.
- Global service locator.
- Complex event bus.
- Generic rule scripting language.
- Universal component/trait framework.
- Large inheritance hierarchy for rules.

Future evidence that would justify some of them:

- Scripting: several real modules require behavior that declarative triggers cannot express cleanly.
- SQLite: saves/campaigns become large enough that ad hoc file loading is slow or query-heavy.
- Event bus: multiple independent in-process consumers need subscription/filtering beyond a returned event vector.
- Plugin ABI: trusted internal extensions need native performance and cannot be expressed as data or scripts.
- ECS: entity variety becomes broad enough that the current `Entity` record blocks development despite traits and content definitions.

## Risks

| Risk | Severity | Why It Matters | Mitigation |
|---|---:|---|---|
| Public API freezes raw internals | High | Modules/frontends could depend on `Game`, `Entity`, `Tile`, and array indexes | Introduce `GameSession`, snapshots, and content IDs before module work |
| Save corruption handling | High | Malformed saves can partially mutate state or allocate huge memory | Harden current save/load first |
| Renderer extraction churn | Medium | Rendering currently reads many internals directly | Add snapshot DTOs incrementally and keep text output behavior stable |
| Event over-design | Medium | Too many event types can slow progress | Start with `LogMessage` plus a few gameplay facts |
| Module schema over-design | Medium | Designing all module features upfront will stall implementation | Build `testing-tomb` first and generalize only concrete needs |
| Name/index coupling | High | Modules and saves cannot safely reference reordered data | Adopt stable content IDs and lookup adapters |
| Sound side effects in rules | Medium | GUI/tests/replay should not fork sound processes | Emit events and let app/frontend consume them |
| Procedural map assumptions | Medium | Module maps will not match procedural `map.cpp` assumptions | Add alternate module map loading path without removing procedural generation |

## Recommended Next Actions

1. Harden `load_game` and `save_path_for_hero` before broader architecture changes.
2. Add a small `GameSession` wrapper that owns `Game` and delegates to current functions.
3. Move the current turn sequencing from `frontends/text/src/main.cpp` into `GameSession::apply`.
4. Define `GameStateSnapshot` with only the fields the text frontend currently needs.
5. Move `render()` out of `engine/src/game.cpp` and render snapshots in the text frontend.
6. Add `GameEvent` with `LogMessage` plus a few core event types, preserving `event_log`.
7. Move sound playback to a simple event consumer in the application/frontend layer.
8. Define `ContentId` and canonical `bx.*` IDs with adapter lookup over existing `bx_data.hpp`.
9. Sketch the `testing-tomb` module JSON files and validation tests before implementing a broad loader.
10. ~~Create the four ADRs listed above once the first boundary work begins.~~ Done in `docs/adr/`.
11. ~~Prove the Testing Tomb module campaign end-to-end.~~ Done through Phase 30: content, gameplay, room B, rumor delivery, campaign save/reload, and a bootable campaign (`gen_pregenerated` seeds `Tomb Runner.dat`; the frontend re-enters it; a dungeon `Save` command supports play → save → quit → reload → resume in-module). The remaining follow-up priorities are gameplay-completeness track items (see `ROADMAP.md`), not module machinery.
12. ~~Undead drain and weapon immunity.~~ Done in Phase 31: wight/wraith touches drain one level (`apply_level_drain` mirrors `check_level_up`; XP drops to the drained band's midpoint), shadow touches drain 1 STR, and shadows/wights/wraiths honor a `WeaponImmunity` tier at spawn (`SilverOrMagic` / `MagicOnly`) in both `attack_internal` and `ranged_attack` via `hero_weapon_can_wound` (silver dagger passes wights; spells, holy water, and turning bypass melee). Still name-derived at spawn like the pre-existing specials; the data-traits refactor stays on the backlog.
13. ~~Rust monster corrosion.~~ Done in Phase 32: a rust monster touch (`corrodes_metal`, set at spawn) corrodes one normal-metal piece per hit via `apply_rust_corrosion` — worn chain/plate, then shield, then the wielded metal weapon (hero left unarmed at 1d2), then carried metal — reusing the same `weapon_counts`/`armor_id`/`weapon_id`/`has_shield` fields the rest of the game writes, so destroyed gear lightens encumbrance and round-trips through save v7. Weapon-material classification (`weapon_is_metal` / `armor_idx_is_metal`) is new and central (silver/wood-of-missile-weapons unharmed); spells, holy water, and turning are unaffected. Next slice per ROADMAP: oozes/slimes ignores-armor damage and green slime dissolve, reusing this machinery.
14. ~~Green slime contact + clinging.~~ Done in Phase 33: `apply_green_slime` (spawn flag in the combat special chain) dissolves the worn non-magical armor, shield, and wielded weapon (wood included; silver survives) and clings for 1d8 per non-scrape round; removal is a new `Scrape` player command (frontend key `s`) that consumes a normal turn, with status mirrored to `HeroView` (save v7 unchanged — it is session state like paralysis). Note the B/X correction: there is no "ignores armor" damage rule in B/X (AC only affects to-hit), so gray ooze/ochre jelly needed no change. Next per ROADMAP: slime fire-only destruction and fire/lightning splitting.
