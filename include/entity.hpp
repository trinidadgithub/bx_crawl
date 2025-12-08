#pragma once

#include <string>

struct Vec2 {
    int x{};
    int y{};
};

enum class EntityType {
    Hero,
    Goblin,
    Kobold
};

struct Entity {
    std::string name;
    EntityType type{};
    Vec2 pos{};
    int max_hp{};
    int hp{};
    int attack_bonus{}; // +to-hit
    int damage_die{};   // e.g. 6 for 1d6
    int ac{};           // ascending AC (higher = harder to hit)
    int morale{};       // 2–12 for monsters; 0 means "no morale checks"
    bool alive{true};
};

