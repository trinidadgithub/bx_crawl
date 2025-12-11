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
};

// Random helper
int roll_d(Game& g, int sides);

// Game lifecycle / logic
void init_game(Game& g);
void render(const Game& g);
void monsters_turn(Game& g);

// Utility: are there any monsters still alive?
bool any_monsters_alive(const Game& g);

// ---------------------------
// Player command interface
// ---------------------------

enum class PlayerCommandType {
    None,
    Move,
    Wait,
    Quit
};

struct PlayerCommand {
    PlayerCommandType type{PlayerCommandType::None};
    int dx{0};
    int dy{0};
};

// Apply a player command to the game state.
// Returns false if the game loop should end (e.g. Quit), true otherwise.
bool apply_player_command(Game& g, const PlayerCommand& cmd);

