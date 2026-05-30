#pragma once

#include <string>
#include <vector>

struct Vec2 {
    int x{};
    int y{};
};

enum class EntityType {
    Hero,
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
    Cleric
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
    std::string name;
    EntityType type{};
    Vec2 pos{};
    int max_hp{};
    int hp{};
    int attack_bonus{};    // +to-hit
    int damage_die{};      // e.g. 6 for 1d6
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

    // Duration counters for buff spells
    int shield_turns{0};
    int protection_turns{0};
    int courage_turns{0};      // Remove Fear — +1 atk
    int resist_cold_turns{0};  // Resist Cold — +1 AC

    // Ranged ammunition
    int knives{};
    int darts{};

    // Spell slots (per level, indexed 0..8)
    int spell_slots[9]{};

    // Known spell names (for Magic-Users / Clerics)
    std::vector<std::string> spells;
};

