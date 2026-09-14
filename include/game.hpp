#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include "dice.hpp"
#include "entity.hpp"
#include "game_event.hpp"
#include "map.hpp"
#include "description.hpp"

enum class GameState {
    Town,
    Tavern,
    Temple,
    Shop,
    Armorer,
    Camp,
    Dungeon,
    CharacterSheet
};

// A rumor the tavern can serve: a module rumor id and its resolved text.
struct GameRumor {
    std::string rumor_id;
    std::string text;

    bool operator==(const GameRumor&) const = default;
};

// Mutable module door runtime state, keyed by the module's stable door id.
// Position, routes, and the author's locked flag are module content; a
// campaign only needs to persist whether the tile sits open and unlocked.
struct ModuleDoorFlag {
    std::string door_id;
    bool open{false};
    bool locked{false};

    bool operator==(const ModuleDoorFlag&) const = default;
};

struct Game {
    int width{};
    int height{};
    std::vector<Tile> map;
    Entity hero;
    std::vector<Entity> monsters;
    // RandomService; see dice.hpp.
    DiceRoller rng;

    DescriptionStore descriptions;
    std::string current_location_id;

    // Data directory for descriptions, etc.
    std::filesystem::path data_dir;

    // Event log
    std::vector<std::string> event_log;
    std::vector<GameEvent> events;

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
    bool dungeon_peace_announced{false};

    // Town / meta-game state
    GameState state{GameState::Town};
    GameState previous_state{GameState::Town};
    int town_submenu{0};

    // Module rumor table bound to the tavern (resolved text) and the rumor ids
    // the tavern has already served this session.
    std::vector<GameRumor> module_rumors;
    std::vector<std::string> heard_rumors;

    // Module identity (empty for the procedural core campaign) and the mutable
    // door states a module campaign must persist across saves.
    std::string module_id;
    std::string module_version;
    std::vector<ModuleDoorFlag> module_door_states;
};

// Random helper
int roll_d(Game& g, int sides);

// Game lifecycle / logic
// Interactive character creation lives in the text frontend
// (frontends/text/src/char_session.cpp); start a game with the finished hero.
void init_game(Game& g, Entity hero);
void init_game(Game& g, Entity hero, const std::filesystem::path& data_dir);

// Attach a finished hero (e.g. from frontend character creation) to an
// already-initialized Game and build the starting dungeon. The RNG is assumed
// to have been seeded already (e.g. via init_game_runtime), so the character
// creation rolls and the dungeon generation share one seed stream.
void start_game_with_hero(Game& g, Entity hero, const std::filesystem::path& data_dir);

void init_game_runtime(Game& g, const std::filesystem::path& data_dir = std::filesystem::path{"../data"});
void render(const Game& g);
void monsters_turn(Game& g);

// Compute field-of-view / torch visibility
void compute_visibility(Game& g);

// Utility: are there any monsters still alive?
bool any_monsters_alive(const Game& g);
void announce_dungeon_at_peace(Game& g);

// Roll 1d6 per side for initiative. Returns true if monsters act first.
bool roll_initiative(Game& g);

struct SurpriseResult {
    int hero_roll{};
    int monster_roll{};
    bool hero_surprised{};
    bool monsters_surprised{};
};

// B/X surprise: each side is surprised on 1-2 on 1d6 by default.
SurpriseResult resolve_surprise(int hero_roll, int monster_roll,
                                int hero_threshold = 2,
                                int monster_threshold = 2);
SurpriseResult roll_surprise(Game& g,
                             int hero_threshold = 2,
                             int monster_threshold = 2);

// Some monsters are unusually stealthy and surprise opponents on better
// odds than 1-2. Returns the hero's surprise threshold (roll <= threshold)
// when facing the named monster. Default is 2.
int monster_surprise_threshold(std::string_view name);

// B/X detailed encumbrance, measured in coins (10 cn ~= 1 lb).
int carried_weight_cn(const Game& g);
int exploration_move_feet(const Game& g);
int encounter_move_feet(const Game& g);
int ticks_per_exploration_turn(const Game& g);

// Save directory helpers
std::filesystem::path saves_directory();
std::filesystem::path save_path_for_hero(const std::string& name);
std::vector<std::filesystem::path> enumerate_saves();
std::string strip_save_extension(std::filesystem::path p);
std::filesystem::path pregenerated_saves_directory(const std::filesystem::path& data_dir);
std::vector<std::filesystem::path> enumerate_all_saves(const std::filesystem::path& data_dir = std::filesystem::path{"../data"});
std::filesystem::path find_save(const std::string& name, const std::filesystem::path& data_dir = std::filesystem::path{"../data"});

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
    ThrowHolyWater,
    Cast,
    Turn,
    Inventory,
    Help,
    Get,
    Use,
    Hide,
    Search,
    Listen
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
