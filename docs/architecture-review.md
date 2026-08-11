# BX Crawl Architecture Review and Extensibility Assessment

## Executive Summary

BX Crawl is in a healthy prototype state, but not yet a clean reusable BX rules engine.

The strongest parts are:

- A real engine library target exists: `bx_engine` in `engine/CMakeLists.txt`.
- Game behavior is testable through `Game`, `PlayerCommand`, and deterministic `std::mt19937` seeds.
- Rules data is partially data-driven through generated/curated `include/bx_data.hpp`.
- The automated test suite is broad and currently passes.

The main architectural issue is concentration. Most gameplay, rules, persistence, rendering, town flow, event messages, spawning, AI, spell effects, save/load, and some diagnostics are centralized in `engine/src/game.cpp`. `Entity` is also a large public bag of mixed PC, monster, inventory, AI, spell, light, and condition state.

The project is not over-engineered. It is under-separated in predictable prototype ways. No rewrite is needed. The right path is incremental extraction behind existing tests.

## Current Architecture

Repository structure:

```text
Repository
|
+-- include/
|   +-- game.hpp          Public Game state and procedural API
|   +-- entity.hpp        Hero/monster/entity state
|   +-- map.hpp           Tiles, rooms, map helpers
|   +-- bx_data.hpp       Generated/curated constexpr B/X data
|   +-- chargen.hpp       Interactive character creation
|   +-- sound.hpp         Procedural sound API
|   +-- description.hpp   Text description store
|
+-- engine/
|   +-- src/game.cpp      Core game logic, rendering, persistence, rules, town flow
|   +-- src/map.cpp       Procedural dungeon map generation
|   +-- src/chargen.cpp   Interactive chargen with std::cin/std::cout
|   +-- src/sound.cpp     Fork/exec sound backend
|   +-- src/description.cpp
|   +-- src/item_effects.cpp
|   +-- src/debug_log.cpp
|
+-- frontends/text/
|   +-- src/main.cpp      POSIX raw terminal loop, key parsing, top-level flow
|
+-- tests/
|   +-- test_core.cpp     Doctest suite, broad gameplay coverage
|
+-- tools/               Utility executables
+-- dnd-data/            Extracted B/X source data, JSON data, schemas, scripts
+-- scripts/gen_data.py  JSON/schema to bx_data.hpp codegen
+-- docker/              Docker packaging
```

Current dependency shape:

```text
Text Frontend
    |
    v
bx_engine public API
    |
    v
Game struct + Entity + Tile + bx_data.hpp
    |
    +-- procedural rules in game.cpp
    +-- procedural map generation in map.cpp
    +-- binary save/load in game.cpp
    +-- terminal rendering in game.cpp
    +-- sound triggers in game.cpp
```

The project currently resembles:

```text
Text Frontend / Loop
    |
    v
Game Procedural API
    |
    v
Single Authoritative Game State
    |
    +-- Rules
    +-- Map
    +-- Persistence
    +-- Presentation text/events
    +-- Sound triggers
```

It does not yet have a separate application layer, stable rules-service boundary, module runtime, persistence abstraction, or frontend-neutral state snapshot.

## Strengths

- `Game` is an explicit authoritative state object, not hidden global game state. This is worth preserving.
- Randomness is mostly centralized through `Game::rng` and `roll_d(Game&, int)`.
- `PlayerCommand` exists, which is a good seed for frontend-independent command input.
- `bx_engine` is already a library target separate from `bx_crawl_text`.
- Tests directly exercise rules behavior through the engine API rather than through terminal input.
- `bx_data.hpp` puts static B/X data in one namespace and avoids runtime database complexity.
- Value semantics dominate. There is very little manual ownership and no obvious heap ownership problem.
- Save files have a magic/version header in `save_game` and `load_game`.
- The codebase is small enough that refactoring can be done safely in thin slices.

## Architectural Issues

The highest-level architectural problem is mixed responsibility in `game.cpp`.

Examples:

- Rules and combat: `attack_internal` in `engine/src/game.cpp`.
- Monster AI: `monsters_turn` in `engine/src/game.cpp`.
- Visibility, time, light, hunger: `compute_visibility` in `engine/src/game.cpp`.
- Terminal rendering: `render` in `engine/src/game.cpp`.
- Binary persistence: `save_game` and `load_game` in `engine/src/game.cpp`.
- Town menu state transitions: `apply_town_command` in `engine/src/game.cpp`.
- Player dungeon command handling: `apply_player_command` in `engine/src/game.cpp`.

This means the engine is currently a game implementation, not yet a reusable rules engine.

Strong boundaries to preserve:

- `map.cpp` is reasonably cohesive for procedural map generation.
- `description.cpp` is small and isolated.
- `item_effects.cpp` is small and focused.
- `frontends/text/main.cpp` owns raw terminal input and the main event loop.

Weak or missing boundaries:

- Rendering is inside engine code via `render(const Game&)`.
- Character creation is interactive inside `engine/src/chargen.cpp`, with `std::cin`/`std::cout` throughout.
- Sound is called directly from rules code.
- Persistence knows the exact binary layout of `Game`/`Entity` fields.
- Data lookup and rule behavior are mixed, especially for spells and monster special cases.
- Environment-variable behavior is read directly inside engine logic, e.g. `BX_NO_MONSTERS` and `BX_TEST_MONSTER`.

## C++ Implementation Review

Memory ownership is mostly sound.

There is little manual allocation:

- No meaningful use of `new`/`delete`.
- `std::vector`, `std::array`, `std::string`, and value types are used heavily.
- `Entity* monster_at(Game&, int, int)` returns non-owning pointers into `g.monsters`, which is acceptable as long as those pointers are short-lived and no `push_back`/reallocation occurs while they are held.

Key risks:

- `read_str` reads an `int32_t len`, casts it to `std::size_t`, and allocates a string without validating negative or huge lengths. A corrupt save can cause very large allocation.
- `load_game` does not consistently check stream state after reads. Partial/corrupt saves can leave partially mutated `Game`.
- `index_for` asserts bounds; in release builds, invalid callers can compute invalid indices. Many callers check `in_bounds`, but module input/save input would increase risk.
- `save_path_for_hero` uses the raw hero name as a filename. A name containing path separators can escape the saves directory.
- `sound.cpp` uses global mutable state and installs `SIGCHLD` handling globally in `sound_init`. This is acceptable for a terminal prototype, but should not live in reusable engine core.
- `sound.cpp` uses `std::rand` separately from `Game::rng`, which is fine for audio noise but reinforces that sound should sit outside deterministic game execution.

Modern C++ usage is pragmatic but can improve:

- `enum class` is used well.
- Value semantics are used well.
- `const` usage is decent but can be strengthened around helper functions.
- Stronger domain types would help for `Hp`, `ArmorClass`, `Coins`, `DungeonLevel`, `Turns`, `TileIndex`, and `MonsterId`, but should be introduced selectively.
- Public structs expose all internals. That is convenient for tests but makes future module APIs unstable.
- Some arrays depend on implicit indexes matching `bx_data`, e.g. `weapon_counts`, `armor_counts`, and `gear_counts`. This is efficient but fragile for module-defined items.

## Rules Engine Review

Current rules are a mix of:

- Data-driven static tables in `bx_data.hpp`.
- Procedural hard-coded logic in `game.cpp`.
- Name-based special cases.
- Some frontend-driven command choices.

Clean examples:

- Ability modifiers and saving throws are table-driven through `bx_data`.
- Surprise has reusable functions: `resolve_surprise`, `roll_surprise`, and `monster_surprise_threshold`.
- Encumbrance helpers are public and testable: `carried_weight_cn`, `exploration_move_feet`, `encounter_move_feet`, `ticks_per_exploration_turn`.

Less extensible examples:

- Monster special traits are name predicates in `game.cpp`, e.g. `is_name_undead`, `is_name_poison`, `is_name_paralysis`, `is_name_normal_bat`, `is_name_giant_bat`.
- Spell resolution is a long chain of string comparisons inside `cast_spell`.
- Class behavior is repeated across switches in `game.cpp`, `chargen.cpp`, and helper functions.
- Monster leader/daylight behavior is currently name-based. It works, but adding more monster families will keep growing conditionals.
- `EntityType` still contains older hard-coded monster types while data-driven monsters use `EntityType::Monster`, which suggests transitional design.

The engine is not yet independently reusable as a BX rules engine because domain state, procedural gameplay, event text, sound, and rendering are intermixed.

Recommended direction:

- Keep `bx_data.hpp` as the initial rules-data source.
- Extract rule functions into small stateless services before adding interfaces.
- Convert name-based monster specials into data traits over time.
- Convert spell string chains into a dispatch table keyed by spell id/name.
- Keep `Game` as the authoritative mutable state for now, but stop making modules/frontends depend on raw fields.

## UI Architecture Review

Replacing the UI today would be possible but not clean.

Good:

- The text frontend already maps terminal input into `PlayerCommand`.
- `apply_player_command` and `apply_town_command` can be reused by tests and future frontends.
- The frontend owns raw terminal mode and the main loop in `frontends/text/src/main.cpp`.

Problems:

- `render(const Game&)` is part of the engine API and writes directly to `std::cout`.
- `render_town_screen` and `render` contain ANSI terminal behavior in `game.cpp`.
- `chargen.cpp` is fully interactive and blocks on `std::getline`.
- The engine calls sound functions directly.
- Game event messages are plain strings stored in `Game::event_log`, which is useful but not structured.

Recommended frontend-neutral model:

```text
Frontend
    |
    v
GameCommand
    |
    v
GameSession::apply(command)
    |
    v
ActionResult
    |
    +-- vector<GameEvent>
    +-- GameStateSnapshot
    +-- requiresInput?
```

Concrete future DTOs:

- `GameCommand`: move, open, close, rest, cast, turn, buy, sell, enter dungeon, save.
- `GameEvent`: structured event type plus text, e.g. `AttackRolled`, `DamageApplied`, `MonsterKilled`, `ItemPickedUp`, `DoorOpened`.
- `GameStateSnapshot`: read-only frontend view of map, entities, HUD, menus, inventory, logs.
- `Prompt`: frontend-neutral request for choices, useful for chargen and shops.

Do not build a huge UI framework now. First step is to move `render()` out of `bx_engine` into the text frontend and replace it with snapshot helpers.

## Persistence Review

There is no database today.

Current persistence:

- Binary save files at `~/.bx_crawl/saves/<hero>.dat`.
- Legacy fallback at `~/.bx_crawl/save.dat`.
- Implemented in `engine/src/game.cpp`.
- Save version currently `5`.
- Save stores mostly hero/meta data, not full dungeon/module/world state.
- Static rules data is compiled into `include/bx_data.hpp`.
- Descriptions are loaded from plain text via `DescriptionStore`.

Important limitations:

- Save/load is tightly coupled to `Entity` and `Game` layout.
- Dungeon map, monster list, room state, traps, explored state, RNG state, module state, and location state are not fully persisted.
- Save data is not safe against malformed files.
- No schema migrations beyond manual version conditionals.
- No distinction yet between rules data, module data, campaign data, save state, and user configuration.

Recommended data ownership model:

| Data Category | Owner | Suggested Format |
|---|---|---|
| Core BX rules data | Engine/rules package | Compiled `bx_data.hpp` initially, later versioned data pack |
| Optional house rules | Engine config / campaign config | JSON/TOML config referencing known rule ids |
| Module data | Module package | JSON manifest plus JSON content files |
| Campaign state | Campaign/save system | Save file or SQLite later |
| Runtime state | Game session | In-memory structs |
| User settings | Frontend/app | Small config file |

Database recommendation:

- Do not introduce SQLite immediately.
- Use JSON module files for authored content first.
- Use a validated save format for campaign state.
- Consider SQLite later only when persistent campaign/module state becomes large or query-heavy.

## Game Module Architecture Proposal

Use data-only modules first. Avoid compiled plugins and scripting at the beginning.

Recommended module package layout:

```text
modules/
  caves-of-chaos/
    module.json
    locations.json
    maps/
      level1.json
      level2.json
    encounters.json
    npcs.json
    treasures.json
    traps.json
    tables.json
    text/
      room_descriptions.txt
    assets/
      optional-files
```

Why this fits this project:

- The project already uses JSON and codegen.
- Current modules/adventures are likely content-heavy, not code-heavy.
- Data-only modules are testable and safe.
- No ABI/plugin burden.
- No scripting sandbox required up front.

Manifest schema:

```json
{
  "module_id": "caves-of-chaos",
  "title": "Caves of Chaos",
  "version": "1.0.0",
  "author": "Author Name",
  "bx_rules_version": "moldvay-basic",
  "engine_min_version": "0.2.0",
  "engine_max_version": null,
  "content_schema_version": 1,
  "entry_point": {
    "location_id": "keep",
    "start_room_id": "gate"
  },
  "dependencies": [],
  "files": {
    "locations": "locations.json",
    "encounters": "encounters.json",
    "npcs": "npcs.json",
    "treasures": "treasures.json",
    "tables": "tables.json"
  },
  "assets": {
    "descriptions": "text/room_descriptions.txt"
  }
}
```

Engine-facing abstractions:

- `GameSession`: owns one running game/campaign session.
- `RulesEngine`: stateless or mostly stateless rule resolution.
- `ContentCatalog`: immutable rules/module data lookup.
- `ModuleLoader`: validates and loads module packages into `ModuleDefinition`.
- `WorldState`: mutable location/room/world flags.
- `SaveService`: serializes campaign/session state.
- `RandomService` or `DiceRoller`: wraps seeded RNG and roll history.
- `EventSink`: receives structured game events.

Module-facing abstractions should be declarative:

```json
{
  "room_id": "b12",
  "description_id": "b12.description",
  "exits": [
    { "direction": "north", "to": "b11", "door": "wooden_locked" }
  ],
  "encounter": {
    "type": "fixed",
    "monsters": [
      { "monster_id": "goblin", "count": "2d4", "faction": "red-hand" }
    ]
  },
  "treasure": [
    { "type": "coins", "gp": "3d6" }
  ],
  "traps": [
    { "trap_id": "covered_pit", "trigger": "enter" }
  ],
  "on_enter": [
    { "if_flag_absent": "b12.entered", "set_flag": "b12.entered" }
  ]
}
```

Scripting recommendation:

- Do not embed Lua/JS yet.
- Start with declarative triggers: `on_enter`, `on_open`, `on_defeat`, `on_take`, `on_timer`.
- Add a small expression/condition system: flags, party/class checks, dice checks, item checks.
- Consider Lua later only if declarative triggers become painful.
- If scripting is added later, sandbox it and expose only a narrow module API.

Compatibility strategy:

- Engine version: semantic version.
- Rules version: explicit string, e.g. `bx-moldvay-basic-1981`.
- Content schema version: integer with migration/validation.
- Module manifest declares min/max engine version.
- Module load fails with structured diagnostics if unsupported.
- Save files store engine version, content schema version, module id/version, rules version, and module state schema version.

## Testing Review

Current test quality is good for a prototype.

Evidence:

- `tests/test_core.cpp` is broad and behavior-focused.
- Tests cover map generation, surprise, paralysis, bats, daylight penalties, leaders, morale, combat, spells, stealth, traps, encumbrance, save/load, town shops, and data tables.
- Tests seed `Game::rng`, making many rules deterministic.
- Local CTest passes.

Weaknesses:

- One very large test file mirrors one very large engine file.
- Tests often manipulate raw `Game`/`Entity` fields directly, which locks in internals.
- There are no module-loading tests because there is no module loader yet.
- Persistence tests cover round-trip success, but not malformed/corrupt save robustness.
- Rendering is not snapshot-tested and is difficult to test because it writes directly to `std::cout`.
- Some behavior is tested by inspecting event-log text, which can make text edits break rules tests.

Recommended testing pyramid:

- Unit tests for pure rules: attack roll, saving throw, morale, search, encounter rolls, spell effects.
- State-transition tests for `GameSession::apply(command)`.
- Snapshot tests for frontend-neutral `GameStateSnapshot`.
- Persistence round-trip and corrupt-input tests.
- Module validation tests once module loading exists.
- Golden module tests: `load_module`, `start_new_game`, `enter_room("B12")`, `perform_action`, assert structured state/events.

## Documentation Review

Existing docs are useful but mostly player/development oriented.

Observed docs:

- `README.md`: build/run instructions, developer flags, controls, town, supplies, save location.
- `Monsters.md`: legacy simplified monster notes.
- `Scrolls.md`: scroll/spell flavor and mechanical notes.
- `dnd-data/AGENTS.md` and `STATION_STATUS.md`: extraction/data pipeline notes.
- JSON schemas under `dnd-data/schemas/`.
- `dnd-data/schemas/10-house-rules.json`: optional Sense Deceit spec.

Missing docs that would add real value:

- `docs/architecture.md`: current layers, ownership, dependency direction.
- `docs/rules-engine.md`: what is RAW, simplified, or optional.
- `docs/module-system.md`: future manifest/content schema once started.
- `docs/persistence.md`: save file versioning and compatibility.
- `docs/testing.md`: deterministic RNG and test patterns.

Avoid documenting speculative task lists in-repo. Design docs should describe current decisions and stable interfaces.

## Security and Robustness Review

Relevant risks:

- Corrupt save files can cause huge allocation via unchecked string length in `read_str`.
- Save file stream failures are not consistently checked after each read.
- Hero names can influence save paths without sanitization.
- Future module files must be treated as untrusted input; currently there is no validation layer.
- Docker packaging appears broken: `docker/Dockerfile` copies `/src/build/frontends/text/bx_crawl`, but the target is `bx_crawl_text`; it also copies `Monsters.md`/`Scrolls.md` from the builder stage without copying them into the builder image first.
- `DescriptionStore::load_from_file` is simple and fine for trusted text, but future module descriptions need size limits and diagnostic errors.
- `std::filesystem::directory_iterator` usage in save enumeration does not handle permission/errors with `std::error_code`; acceptable locally, weak for robustness.
- No SQL injection risk because there is no SQL/database use.

## Target Architecture

Realistic target architecture:

```text
+------------------------------------------------+
| Frontends                                      |
| Text UI | Future TUI | GUI | Web               |
+-----------------------+------------------------+
                        |
                        v
+------------------------------------------------+
| Application Layer                              |
| GameSession | Command Router | Snapshots       |
| Prompts | Frontend DTOs                       |
+-----------------------+------------------------+
                        |
                        v
+------------------------------------------------+
| BX Engine                                      |
| State transitions | Combat | Exploration       |
| Time | Encounters | Inventory | Magic          |
+-----------+-------------------+----------------+
            |                   |
            v                   v
+--------------------+   +-----------------------+
| Domain Model       |   | Content Catalog        |
| Character | Entity |   | Core BX data           |
| Map | Room | Item  |   | Module definitions     |
| Conditions | State |   | Random tables          |
+--------------------+   +-----------------------+
            |                   ^
            v                   |
+--------------------+   +-----------------------+
| Persistence        |   | Module Loader          |
| Saves | Campaigns  |   | Manifest validation    |
| Module state       |   | Content parsing        |
+--------------------+   +-----------------------+
```

Dependency direction:

```text
Frontend -> Application -> Engine -> Domain
Application -> Persistence interfaces
Application -> ModuleLoader
Engine -> ContentCatalog interfaces
Engine -> Random/Dice service
Domain -> no frontend, no persistence, no module loader
```

Public API boundaries:

- Stable: `GameCommand`, `ActionResult`, `GameEvent`, `GameStateSnapshot`, `ModuleManifest`, `ModuleDefinition`, `DiceRoller`.
- Semi-stable: `GameSession`, `SaveService`, `ModuleLoader`.
- Internal: raw `Entity`, raw `Tile`, raw `Game`, spell implementation details, monster AI helpers, save binary details.

Initial migration target:

```cpp
struct ActionResult {
    bool keep_playing;
    std::vector<GameEvent> events;
};

class GameSession {
public:
    ActionResult apply(const GameCommand&);
    GameStateSnapshot snapshot() const;
};
```

This can wrap current `Game` without rewriting it.

## Refactoring Roadmap

Immediate:

- Harden `load_game`: validate string lengths, check stream state, reject invalid enum values, avoid partial mutation by loading into a temporary `Game`.
- Sanitize save filenames derived from hero names.
- Fix Docker packaging if Docker is intended to work.
- Keep adding behavior tests before each extraction.

Near-term:

- Move `render()` and `render_town_screen()` out of `engine/src/game.cpp` into the text frontend.
- Split `game.cpp` into cohesive source files without changing public behavior: combat, spells, exploration, town, persistence, rendering.
- Move `chargen.cpp` toward a non-interactive character builder plus a text UI wrapper.
- Introduce structured `GameEvent` while still preserving readable log strings.
- Extract `DiceRoller` or `RandomService` around `Game::rng`.

Before module support:

- Define module manifest and validation errors.
- Add `ContentCatalog` lookup by stable ids rather than array indexes.
- Separate immutable content definitions from mutable runtime state.
- Replace gear/weapon/armor fixed-count arrays with id-based inventory or validated content indexes.
- Persist full campaign/module state, including current module id/version and world flags.
- Add module-loading tests with one tiny sample module.

Future:

- Add declarative event triggers.
- Add richer save/campaign format.
- Add scripting only if declarative modules prove insufficient.
- Consider SQLite only when save/module state becomes query-heavy or large.

## Risk Matrix

| Area | Risk | Severity | Evidence | Recommendation |
|---|---:|---:|---|---|
| Save parsing | Corrupt save can allocate huge string or partially mutate state | High | `read_str` lacks length validation | Validate lengths, stream state, load into temporary state |
| UI coupling | Engine not frontend-neutral | High | `render()` and ANSI output in engine | Move rendering to frontend, expose snapshots |
| Monolithic engine | Changes require editing unrelated systems | High | `game.cpp` is very large and multi-purpose | Split by behavior behind existing tests |
| Rules extensibility | New spells/monster traits require editing central conditionals | Medium | `cast_spell` string chain; monster name predicates | Add trait data and spell dispatch table |
| Module readiness | External adventures cannot load without hard-coded engine edits | High | No module loader/catalog/runtime state separation | Add data-only module manifest and content catalog |
| Persistence completeness | Save/load does not preserve full dungeon/world state | Medium | Save stores hero/meta, not full map/monsters/module flags | Define campaign save model before modules |
| Randomness | Mostly good, but not abstracted for replay | Low | `Game::rng` is explicit; sound uses separate `rand` | Keep game RNG, add roll history/seed service later |
| Sound coupling | Engine calls POSIX sound backend | Medium | `sound_*` calls inside rules; `fork/exec` in `sound.cpp` | Move sound to event consumer/frontend |
| Build packaging | Docker likely broken | Medium | Docker copies wrong target name | Fix Dockerfile or remove stale docker support |
| Public API stability | Internals exposed as API | Medium | Public mutable `Game` and `Entity` fields | Add wrapper APIs before modules depend on internals |

## Recommended Next 10 Actions

1. Harden save loading against malformed input and path traversal.
2. Fix or remove stale Docker packaging.
3. Move terminal rendering out of `bx_engine` into `frontends/text`.
4. Add `GameStateSnapshot` for frontend rendering.
5. Add structured `GameEvent` while keeping current `event_log` text as derived output.
6. Split `game.cpp` into `combat.cpp`, `spells.cpp`, `exploration.cpp`, `town.cpp`, and `save.cpp` without changing APIs first.
7. Extract character creation into non-interactive generation/selection functions, leaving prompts in the text frontend.
8. Introduce a `DiceRoller` wrapper around `Game::rng` for deterministic tests and future replay.
9. Define a minimal data-only module manifest and validation result type.
10. Add a tiny sample module test that loads content and enters one room, even before full module gameplay exists.
