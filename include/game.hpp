#pragma once

#include <random>
#include <vector>
#include <string>
#include "entity.hpp"
#include "map.hpp"
#include "description.hpp"

struct Game {
    int width{};
    int height{};
    std::vector<Tile> map;
    Entity hero;
    std::vector<Entity> monsters;
    std::mt19937 rng;

    DescriptionStore descriptions;
    std::string current_location_id;

    // Event log
    std::vector<std::string> event_log;

    // Progression
    int total_gold{};
    int total_xp{};
    int hero_level{1};
};

// Random helper
int roll_d(Game& g, int sides);

// Game lifecycle / logic
void init_game(Game& g);
void render(const Game& g);
void monsters_turn(Game& g);

// Compute field-of-view / torch visibility
void compute_visibility(Game& g);

// Utility: are there any monsters still alive?
bool any_monsters_alive(const Game& g);

// Roll 1d6 per side for initiative. Returns true if monsters act first.
bool roll_initiative(Game& g);

// ---------------------------
// Player command interface
// ---------------------------

enum class PlayerCommandType {
    None,
    Move,
    Wait,
    Quit,
    Open,
    Close,
    Rest,
    ThrowKnife,
    ThrowDart,
    Cast,
    Turn,
    Help,
    Get
};

struct PlayerCommand {
    PlayerCommandType type{PlayerCommandType::None};
    int dx{0};
    int dy{0};
    int spell_index{};
};

// Apply a player command to the game state.
// Returns false if the game loop should end (e.g. Quit), true otherwise.
bool apply_player_command(Game& g, const PlayerCommand& cmd);

// Class name helper
const char* class_name(CharacterClass cc);

