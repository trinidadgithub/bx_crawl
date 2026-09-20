#include "game_session.hpp"

#include "game.hpp"
#include "game_internal.hpp"
#include "map.hpp"
#include "bx_data.hpp"
#include "content_id.hpp"
#include "item_catalog.hpp"
#include "rules_catalog.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace {

void append_log(Game& g, std::string text) {
    g.event_log.push_back(text);
    g.events.push_back(GameEvent{GameEventType::LogMessage, std::move(text)});
}

// "pit" | "dart" | "poison_dart" — callers validate the string first.
TrapType trap_type_from_module_type(const std::string& type) {
    if (type == "dart")
        return TrapType::Dart;
    if (type == "poison_dart")
        return TrapType::PoisonDart;
    return TrapType::Pit;
}

bool walkable_tile(const Game& g, int x, int y) {
    if (x < 0 || y < 0 || x >= g.width || y >= g.height)
        return false;
    const TileType type =
        g.map[static_cast<std::size_t>(y * g.width + x)].type;
    return type == TileType::Floor || type == TileType::DoorOpen ||
           type == TileType::StairsUp || type == TileType::StairsDown;
}

// Place a monster near (x, y), preferring the requested tile and then the
// adjacent tiles. Fails when every candidate is blocked.
bool place_module_monster(Game& g, Entity& e, int x, int y) {
    const int offsets[][2] = {{0, 0}, {0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    for (const auto& off : offsets) {
        const int px = x + off[0];
        const int py = y + off[1];
        if (!walkable_tile(g, px, py))
            continue;
        if (game_internal::is_occupied(g, px, py))
            continue;
        e.pos = {px, py};
        return true;
    }
    return false;
}

} // namespace

void GameSession::prepare_dungeon_frame() {
    compute_visibility(game_);
}

ActionResult GameSession::apply(const PlayerCommand& cmd) {
    Game& g = game_;
    ActionResult result;

    // Non-dungeon flow: single-command delegation into the town handler. This
    // is the counterpart of the former town branch of the text-frontend loop.
    if (g.state != GameState::Dungeon) {
        result.keep_playing = apply_town_command(g, cmd);
        return result;
    }

    // Non-turn commands: applied directly without initiative or monster action,
    // matching the former frontend special-case. Initiative and monster turns
    // therefore only run for commands that pass a dungeon turn.
    if (cmd.type == PlayerCommandType::Inventory ||
        cmd.type == PlayerCommandType::Save ||
        cmd.type == PlayerCommandType::Quit) {
        result.keep_playing = apply_player_command(g, cmd);
        return result;
    }

    const bool monsters_present = any_monsters_alive(g);
    const bool monsters_first = monsters_present && roll_initiative(g);

    if (monsters_first) {
        monsters_turn(g);
        if (!g.hero.alive) {
            result.keep_playing = false;
            result.hero_died = true;
            result.died_before_action = true;
            return result;
        }
    }

    const std::string prev_location = g.current_location_id;
    result.keep_playing = apply_player_command(g, cmd);
    if (!result.keep_playing)
        return result;

    // The hero may have left the dungeon (stairs): the turn cycle ends here and
    // dungeon-only checks are skipped.
    if (g.state != GameState::Dungeon)
        return result;

    if (!g.hero.alive) {
        result.keep_playing = false;
        result.hero_died = true;
        return result;
    }

    // Module maps have no north/south location split: the module door routes
    // drive location transitions, so re-apply them after each move. The core's
    // procedural update has already run inside the move by this point.
    if (module_mode_ && cmd.type == PlayerCommandType::Move)
        resolve_module_location(prev_location);

    // Keep the campaign-facing door flags in step with the runtime tiles
    // whenever a dungeon command may have opened, closed, or picked a door.
    if (module_mode_)
        sync_module_door_states();

    if (!any_monsters_alive(g))
        announce_dungeon_at_peace(g);

    if (monsters_present && !monsters_first) {
        monsters_turn(g);
        if (!g.hero.alive) {
            result.keep_playing = false;
            result.hero_died = true;
        }
    }

    return result;
}

GameStateSnapshot GameSession::snapshot() const {
    return make_game_state_snapshot(game_);
}

ModuleStartResult GameSession::start_module(const ModuleContent& module, Entity hero) {
    const ModuleLocation* entry = module.entry_location();
    if (entry == nullptr)
        return {false, "module entry location not found"};

    const ModuleMapDefinition* map = module.find_map(entry->map_id);
    if (map == nullptr)
        return {false, "module entry map not found"};
    if (map->width <= 0 || map->height <= 0)
        return {false, "module entry map has invalid dimensions"};
    if (map->entry_x < 0 || map->entry_y < 0 || map->entry_x >= map->width ||
        map->entry_y >= map->height) {
        return {false, "module entry position is out of bounds"};
    }
    if (map->tiles.size() != static_cast<std::size_t>(map->width * map->height))
        return {false, "module entry map tile count mismatch"};

    const TileType* entry_tile = map->tile_at(map->entry_x, map->entry_y);
    if (entry_tile == nullptr || *entry_tile == TileType::Wall || *entry_tile == TileType::DoorClosed ||
        *entry_tile == TileType::SecretDoorClosed) {
        return {false, "module entry position is blocked"};
    }

    game_ = Game{};
    init_game_runtime(game_);
    game_.module_id = module.manifest.module_id;
    game_.module_version = module.manifest.version;
    game_.width = map->width;
    game_.height = map->height;
    game_.map.assign(map->tiles.size(), Tile{});
    for (std::size_t i = 0; i < map->tiles.size(); ++i) {
        game_.map[i].type = map->tiles[i];
    }

    game_.hero = std::move(hero);
    game_.hero.pos = {map->entry_x, map->entry_y};
    game_.total_gold = game_.hero.gold_pieces;
    game_.hero.gold_pieces = 0;
    game_.state = GameState::Dungeon;
    game_.previous_state = GameState::Town;
    game_.dungeon_level = 1;
    game_.current_location_id = entry->location_id;
    game_.wander_counter = 0;
    game_.tick_clock = 0;
    game_.dungeon_peace_announced = false;
    game_.monsters.clear();
    game_.rooms.clear();

    Tile& start = game_.map[static_cast<std::size_t>(map->entry_y * map->width + map->entry_x)];
    start.explored = true;
    start.visible = true;

    // Spawn scripted encounters. Module data stays data-only: encounters name
    // monsters by stable content id, resolved here through the rules catalog.
    for (const ModuleEncounter& encounter : module.encounters) {
        const bx::Monster* monster =
            RulesCatalog::core().find_monster(ContentId{encounter.monster_content_id});
        if (monster == nullptr) {
            game_ = Game{};
            return {false, "encounter references unknown monster " + encounter.monster_content_id};
        }
        const auto slot = bx::monster_index(monster->name);
        if (!slot) {
            game_ = Game{};
            return {false, "encounter monster not found in rules catalog"};
        }

        for (int i = 0; i < encounter.count; ++i) {
            Entity e = game_internal::spawn_bx_monster_by_index(game_, static_cast<int>(*slot));
            if (!place_module_monster(game_, e, encounter.x, encounter.y)) {
                game_ = Game{};
                return {false, "encounter is blocked: " + encounter.encounter_id};
            }
            game_.monsters.push_back(std::move(e));
        }
    }

    // Seed scripted treasures onto the runtime map. Only tile items are laid
    // down; pickup state matches the core game, which resolves coin/gem value
    // and logging at the Get command.
    for (const ModuleTreasure& treasure : module.treasures) {
        Tile& tile = game_
            .map[static_cast<std::size_t>(treasure.y * game_.width + treasure.x)];
        const ItemType item = treasure.type == "gold" ? ItemType::Gold : ItemType::Gem;
        for (int i = 0; i < treasure.count; ++i)
            tile.items.push_back(item);
    }

    // Seed scripted traps onto the runtime map. The core hero_move / search
    // rules read only the tile trap state, so trigger, saving throw, damage,
    // and disarming resolution match the procedural dungeon unchanged.
    for (const ModuleTrap& trap : module.traps) {
        Tile& tile = game_
            .map[static_cast<std::size_t>(trap.y * game_.width + trap.x)];
        tile.has_trap = true;
        tile.trap_type = trap_type_from_module_type(trap.type);
    }

    // Plant module door state and remember the declared door routes. The core
    // Open/Close commands and movement blocking read the tile state; the routes
    // drive module location transitions when the hero crosses a door.
    module_door_routes_.clear();
    for (const auto& [door_id, door] : module.doors) {
        (void)door_id;
        Tile& tile = game_
            .map[static_cast<std::size_t>(door.y * game_.width + door.x)];
        tile.locked = door.locked;
        module_door_routes_.push_back(ModuleDoorRoute{
            door.door_id, door.x, door.y, door.from_location_id, door.to_location_id});
    }
    module_mode_ = true;

    // Bind the module's tavern rumor table. Rumor text resolves from the
    // declared module descriptions; heard/not-heard remains session state the
    // tavern tracks (saving that state is a later campaign-save slice).
    game_.module_rumors.clear();
    for (const ModuleRumor& rumor : module.rumors) {
        const std::string* text = module.find_description(rumor.description_id);
        if (text == nullptr) {
            game_ = Game{};
            return {false, "rumor references missing description"};
        }
        game_.module_rumors.push_back(GameRumor{rumor.rumor_id, *text});
    }

    append_log(game_, "You enter " + module.manifest.title + ".");
    sync_module_door_states();
    return {true, {}};
}

const std::vector<GameEvent>& GameSession::events() const {
    return game_.events;
}

ModuleStartResult GameSession::resume_module(const ModuleContent& module, Entity hero,
                                             const CampaignState& campaign) {
    ModuleStartResult started = start_module(module, std::move(hero));
    if (!started.ok)
        return started;

    Game& g = game_;
    // start_module re-resolves the rumor texts from module content, which is
    // authoritative; only heard-ness is true campaign state.
    g.heard_rumors = campaign.heard_rumors;
    g.current_location_id = campaign.current_location_id;

    // A persistent hero position resumes on the saved tile; the entry
    // position is the fallback for campaigns without a usable recorded spot
    // (e.g. pre-v7 saves, whose position defaults to {0, 0} on a wall).
    if (campaign.hero.pos.x >= 0 && campaign.hero.pos.y >= 0 &&
        campaign.hero.pos.x < game_.width && campaign.hero.pos.y < game_.height) {
        const Tile& tile = game_.map[static_cast<std::size_t>(
            campaign.hero.pos.y * game_.width + campaign.hero.pos.x)];
        const TileType type = tile.type;
        if (type != TileType::Wall && type != TileType::DoorClosed &&
            type != TileType::SecretDoorClosed) {
            game_.hero.pos = campaign.hero.pos;
        }
    }

    // Re-apply persisted door flags onto the freshly rebuilt map.
    for (const ModuleDoorFlag& flag : campaign.module_door_states) {
        for (const ModuleDoorRoute& route : module_door_routes_) {
            if (route.door_id != flag.door_id)
                continue;
            Tile& tile = game_
                .map[static_cast<std::size_t>(route.y * game_.width + route.x)];
            if (flag.open)
                tile.type = TileType::DoorOpen;
            tile.locked = flag.locked;
            break;
        }
    }
    sync_module_door_states();
    return {true, {}};
}

void GameSession::sync_module_door_states() {
    game_.module_door_states.clear();
    game_.module_door_states.reserve(module_door_routes_.size());
    for (const ModuleDoorRoute& route : module_door_routes_) {
        const Tile& tile = game_
            .map[static_cast<std::size_t>(route.y * game_.width + route.x)];
        game_.module_door_states.push_back(
            ModuleDoorFlag{route.door_id, tile.type == TileType::DoorOpen, tile.locked});
    }
}

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
