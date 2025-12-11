#include "chargen.hpp"
#include "game.hpp"

#include <iostream>
#include <string>
#include <algorithm>

// Simple B/X-ish ability modifier:
// 3: -3
// 4-5: -2
// 6-8: -1
// 9-12: 0
// 13-15: +1
// 16-17: +2
// 18: +3
static int ability_modifier(int score) {
    if (score <= 3) return -3;
    if (score <= 5) return -2;
    if (score <= 8) return -1;
    if (score <= 12) return 0;
    if (score <= 15) return 1;
    if (score <= 17) return 2;
    return 3;
}

// Roll 3d6 using the game's RNG
static int roll_3d6(Game& g) {
    return roll_d(g, 6) + roll_d(g, 6) + roll_d(g, 6);
}

Entity create_fighter_hero(Game& g) {
    Entity hero;
    hero.type = EntityType::Hero;
    hero.char_class = CharacterClass::Fighter;

    // Name
    std::cout << "=== Character Creation ===\n\n";
    std::cout << "Enter your hero's name: ";
    std::string name;
    std::getline(std::cin, name);
    if (name.empty()) {
        name = "Unnamed Fighter";
    }
    hero.name = name;

    // Abilities
    hero.abilities.str          = roll_3d6(g);
    hero.abilities.dex          = roll_3d6(g);
    hero.abilities.con          = roll_3d6(g);
    hero.abilities.intelligence = roll_3d6(g);
    hero.abilities.wis          = roll_3d6(g);
    hero.abilities.cha          = roll_3d6(g);

    int str_mod = ability_modifier(hero.abilities.str);
    int dex_mod = ability_modifier(hero.abilities.dex);
    int con_mod = ability_modifier(hero.abilities.con);

    // Level 1 Fighter:
    // HP: 1d8 + CON mod (min 1)
    int hp_roll = roll_d(g, 8);
    hero.max_hp = std::max(1, hp_roll + con_mod);
    hero.hp     = hero.max_hp;

    // Attack bonus: +1 base for Fighter + STR mod (very rough B/X-ish)
    hero.attack_bonus = 1 + str_mod;

    // Damage: 1d8 for Fighter
    hero.damage_die = 8;

    // AC: start from 14 (like chain armor) + DEX mod (ascending AC)
    hero.ac = 14 + dex_mod;

    hero.morale           = 0;    // PCs don't use morale
    hero.fleeing          = false;
    hero.alive            = true;
    hero.attacks_per_turn = 1;

    // For now, position will be set by init_game (map logic).

    // Show a mini character sheet
    std::cout << "\n=== Hero Created ===\n";
    std::cout << "Name: " << hero.name << "\n";
    std::cout << "Class: Fighter (Level 1)\n\n";

    std::cout << "STR: " << hero.abilities.str
              << " (mod " << str_mod << ")\n";
    std::cout << "DEX: " << hero.abilities.dex
              << " (mod " << dex_mod << ")\n";
    std::cout << "CON: " << hero.abilities.con
              << " (mod " << con_mod << ")\n";
    std::cout << "INT: " << hero.abilities.intelligence << "\n";
    std::cout << "WIS: " << hero.abilities.wis << "\n";
    std::cout << "CHA: " << hero.abilities.cha << "\n\n";

    std::cout << "HP: " << hero.hp << "/" << hero.max_hp << "\n";
    std::cout << "Attack bonus: " << hero.attack_bonus << "\n";
    std::cout << "Damage: 1d" << hero.damage_die << "\n";
    std::cout << "AC (ascending): " << hero.ac << "\n\n";

    std::cout << "Press Enter to enter the dungeon...";
    std::string dummy;
    std::getline(std::cin, dummy);

    return hero;
}

