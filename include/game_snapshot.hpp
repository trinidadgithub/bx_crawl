#pragma once

#include "entity.hpp"
#include "game.hpp"
#include "map.hpp"

#include <array>
#include <string>
#include <vector>

struct TileView {
    TileType type{TileType::Floor};
    bool explored{false};
    bool visible{false};
    bool secret_revealed{false};
    bool trap_visible{false};
    bool has_trap{false};
    bool visible_wall{false};
    std::string wall_glyph{" "};
    std::vector<ItemType> items;
};

struct EntityView {
    std::string name;
    EntityType type{EntityType::Monster};
    Vec2 pos{};
    bool alive{true};
    char glyph{'?'};
};

struct HeroView {
    std::string name;
    CharacterClass char_class{CharacterClass::None};
    std::string class_name;
    Vec2 pos{};
    bool alive{true};

    int hp{};
    int max_hp{};
    int ac{};
    int effective_ac{};
    int buff_turns{};
    int attack_bonus{};
    int damage_die{};
    int damage_bonus{};

    Abilities abilities{};
    std::string weapon_id;
    std::string armor_id;
    bool has_shield{false};

    std::array<int, 16> weapon_counts{};
    std::array<int, 4> armor_counts{};
    std::array<int, 20> gear_counts{};
    std::vector<std::string> spells;

    int knives{};
    int darts{};
    int torch_turns{};
    int lantern_turns{};
    int light_spell_turns{};
    int turns_without_food{};
    int rations{};
    int shield_turns{};
    int protection_turns{};
    int courage_turns{};
    int resist_cold_turns{};
    int unconscious_turns{};
    int green_slime_turns{};
    int blister_turns{};
    bool confused{false};
};

struct GameStateSnapshot {
    GameState state{GameState::Town};
    GameState previous_state{GameState::Town};
    int town_submenu{};

    int width{};
    int height{};
    std::vector<TileView> tiles;
    HeroView hero;
    std::vector<EntityView> monsters;

    int total_gold{};
    int total_xp{};
    int hero_level{1};
    int dungeon_level{1};
    int next_level_xp{};
    int alive_monster_count{};

    int carried_weight_cn{};
    int exploration_move_feet{};
    int encounter_move_feet{};

    std::string purse_text;
    std::vector<std::string> event_log;
};

GameStateSnapshot make_game_state_snapshot(const Game& g);
