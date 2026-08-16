#include "campaign_state.hpp"

CampaignState capture_campaign_state(const Game& game) {
    CampaignState state;
    state.current_location_id = game.current_location_id;
    state.state = game.state;
    state.previous_state = game.previous_state;
    state.town_submenu = game.town_submenu;
    state.total_gold = game.total_gold;
    state.total_xp = game.total_xp;
    state.hero_level = game.hero_level;
    state.dungeon_level = game.dungeon_level;
    state.wander_counter = game.wander_counter;
    state.tick_clock = game.tick_clock;
    state.dungeon_peace_announced = game.dungeon_peace_announced;
    state.rng = game.rng;
    state.hero = game.hero;
    state.width = game.width;
    state.height = game.height;
    state.map = game.map;
    state.monsters = game.monsters;
    state.rooms = game.rooms;
    state.event_log = game.event_log;
    return state;
}

void restore_campaign_state(Game& game, const CampaignState& state) {
    game.current_location_id = state.current_location_id;
    game.state = state.state;
    game.previous_state = state.previous_state;
    game.town_submenu = state.town_submenu;
    game.total_gold = state.total_gold;
    game.total_xp = state.total_xp;
    game.hero_level = state.hero_level;
    game.dungeon_level = state.dungeon_level;
    game.wander_counter = state.wander_counter;
    game.tick_clock = state.tick_clock;
    game.dungeon_peace_announced = state.dungeon_peace_announced;
    game.rng = state.rng;
    game.hero = state.hero;
    game.width = state.width;
    game.height = state.height;
    game.map = state.map;
    game.monsters = state.monsters;
    game.rooms = state.rooms;
    game.event_log = state.event_log;

    // Structured events are transient delivery records, not campaign history.
    game.events.clear();
}
