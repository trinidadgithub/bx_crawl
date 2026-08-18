#include "game_session.hpp"

#include "game.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace {

void append_log(Game& g, std::string text) {
    g.event_log.push_back(text);
    g.events.push_back(GameEvent{GameEventType::LogMessage, std::move(text)});
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

    append_log(game_, "You enter " + module.manifest.title + ".");
    return {true, {}};
}

const std::vector<GameEvent>& GameSession::events() const {
    return game_.events;
}
