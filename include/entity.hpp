#pragma once

#include <array>
#include <string>
#include <vector>

struct Vec2 {
    int x{};
    int y{};
};

enum class EntityType {
    Hero,
    Monster, // generic catch-all for bx_data monsters
    Goblin,
    Kobold,
    GiantRat,
    Skeleton,
    Orc,
    GiantSpider,
    Zombie
};

// Character class for PCs (monsters can ignore this for now)
enum class CharacterClass {
    None,
    Fighter,
    MagicUser,
    Thief,
    Cleric,
    Dwarf,
    Elf,
    Halfling
};

struct Abilities {
    int str{};
    int dex{};
    int con{};
    int intelligence{};
    int wis{};
    int cha{};
};

struct Entity {
    static constexpr int MaxSpellLevel = 9;

    std::string name;
    EntityType type{};
    Vec2 pos{};
    int max_hp{};
    int hp{};
    int attack_bonus{};    // +to-hit (melee: includes STR)
    int damage_die{};      // e.g. 6 for 1d6
    int damage_bonus{};    // STR damage modifier (melee)
    int ac{};              // ascending AC (higher = harder to hit)
    int morale{};          // 2–12 for monsters; 0 = no morale checks
    bool fleeing{false};   // if true, tries to escape instead of fight
    bool alive{true};
    int attacks_per_turn{1}; // number of attacks per turn

    // New: character generation bits
    Abilities abilities{};
    CharacterClass char_class{CharacterClass::None};

    // Treasure carried (monsters)
    int gold_pieces{};

    // Charmed (Charm Person)
    bool charmed{false};

    // Stealth (Thief: Hide in Shadows)
    bool hidden{false};

    // Thief skill percentages (B/X level 1 base values)
    int skill_hide{0};         // Hide in Shadows  10%
    int skill_pick_locks{0};   // Open Locks       15%
    int skill_find_traps{0};   // Remove Traps     10%
    int skill_climb{0};        // Climb Walls      85%
    int skill_move_silent{0};  // Move Silently    20%
    int skill_hear_noise{0};   // Hear Noise      33% (1-2 on 1d6)

    // Duration counters for buff spells
    int shield_turns{0};
    int protection_turns{0};
    int courage_turns{0};      // Remove Fear — +1 atk
    int resist_cold_turns{0};  // Resist Cold — +1 AC

    // Light / torch
    int torch_turns{60};
    int light_spell_turns{0};

    // Ranged ammunition
    int knives{};
    int darts{};

    // Spell slots by spell level. Index 0 is unused; valid spell levels are 1..9.
    std::array<int, MaxSpellLevel + 1> spell_slots{};
    std::array<int, MaxSpellLevel + 1> max_spell_slots{};

    // Known spell names (for Magic-Users / Clerics)
    std::vector<std::string> spells;

    // Data-driven monster fields (used with EntityType::Monster)
    char glyph{'?'};
    int xp_value{};
    bool undead{false};
    bool has_poison{false};
    bool has_disease{false};

    // Equipment (weapon + armor IDs from bx_data.hpp)
    std::string weapon_id;
    std::string armor_id;
    bool has_shield{false};

    // Supplies
    std::array<int, 20> gear_counts{};
    int rations{};
    int turns_without_food{};
    int oil_flasks{};
    int holy_water{};

    // Monster AI state
    int ai_patrol_x{}, ai_patrol_y{};
    int ai_wander_timer{};
};
