#pragma once

#include "dice.hpp"
#include "entity.hpp"
#include "game.hpp"
#include "map.hpp"

#include <string>
#include <vector>

struct CampaignState {
    int schema_version{1};
    std::string rules_version{"bx.moldvay-basic"};

    // Module identity when this campaign runs a module; empty for the
    // procedural core campaign. current_location_id carries the hero's
    // position within the module's location graph.
    std::string module_id;
    std::string module_version;
    std::string current_location_id;

    // Module runtime state a campaign must persist: the bound rumor table,
    // the rumor ids already heard, and the module's mutable door tiles.
    std::vector<GameRumor> module_rumors;
    std::vector<std::string> heard_rumors;
    std::vector<ModuleDoorFlag> module_door_states;

    GameState state{GameState::Town};
    GameState previous_state{GameState::Town};
    int town_submenu{};

    int total_gold{};
    int total_xp{};
    int hero_level{1};
    int dungeon_level{1};
    int wander_counter{};
    int tick_clock{};
    bool dungeon_peace_announced{false};

    DiceRoller rng;
    Entity hero;

    int width{};
    int height{};
    std::vector<Tile> map;
    std::vector<Entity> monsters;
    std::vector<Room> rooms;
    std::vector<std::string> event_log;
};

CampaignState capture_campaign_state(const Game& game);
void restore_campaign_state(Game& game, const CampaignState& state);
