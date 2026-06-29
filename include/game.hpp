#pragma once

#include <filesystem>
#include <random>
#include <string>
#include <vector>
#include "entity.hpp"
#include "map.hpp"
#include "description.hpp"

enum class GameState {
    Town,
    Tavern,
    Temple,
    Shop,
    Armorer,
    Camp,
    Dungeon
};

struct Game {
    int width{};
    int height{};
    std::vector<Tile> map;
    Entity hero;
    std::vector<Entity> monsters;
    std::mt19937 rng;

    DescriptionStore descriptions;
    std::string current_location_id;

    // Data directory for descriptions, etc.
    std::filesystem::path data_dir;

    // Event log
    std::vector<std::string> event_log;

    // Progression
    int total_gold{};
    int total_xp{};
    int hero_level{1};
    int dungeon_level{1};

    // Wandering monster timer (checked every ~8 actions)
    int wander_counter{};

    // Turn clock: 10 actions = 1 B/X turn (torch/light decrement on turn boundary)
    int tick_clock{};

    // Dungeon rooms (used for wandering monster placement)
    std::vector<Room> rooms;

    // Town / meta-game state
    GameState state{GameState::Town};
    int town_submenu{0};
};

// Random helper
int roll_d(Game& g, int sides);

// Game lifecycle / logic
void init_game(Game& g);
void init_game(Game& g, const std::filesystem::path& data_dir);
void init_game(Game& g, Entity hero);
void init_game(Game& g, Entity hero, const std::filesystem::path& data_dir);
void render(const Game& g);
void monsters_turn(Game& g);

// Compute field-of-view / torch visibility
void compute_visibility(Game& g);

// Utility: are there any monsters still alive?
bool any_monsters_alive(const Game& g);

// Roll 1d6 per side for initiative. Returns true if monsters act first.
bool roll_initiative(Game& g);

// ---------------------------
// Town / meta-game actions
// ---------------------------

// Enter the dungeon from camp (generates fresh dungeon level 1)
void enter_dungeon(Game& g);

// Leave dungeon via entrance stairs (return to camp)
void leave_dungeon(Game& g);

// Save/load
bool save_game(const Game& g, const std::filesystem::path& path);
bool load_game(Game& g, const std::filesystem::path& path);

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
    Get,
    Use,
    Hide,
    Search
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

// Apply a town command (menu selection).
// Returns false to quit, true otherwise.
bool apply_town_command(Game& g, const PlayerCommand& cmd);

// Class name helper
const char* class_name(CharacterClass cc);

// Equipment helpers (used by both game.cpp and main.cpp)
bool is_weapon_allowed(CharacterClass cc, const char* weapon_id);
