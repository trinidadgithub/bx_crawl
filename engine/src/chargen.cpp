#include "chargen.hpp"
#include "game.hpp"

#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>

// Simple B/X-ish ability modifier:
// 3: -3
// 4-5: -2
// 6-8: -1
// 9-12: 0
// 13-15: +1
// 16-17: +2
// 18: +3
static int ability_modifier(int score) {
    if (score <= 3)  return -3;
    if (score <= 5)  return -2;
    if (score <= 8)  return -1;
    if (score <= 12) return 0;
    if (score <= 15) return 1;
    if (score <= 17) return 2;
    return 3;
}

// Roll 3d6 using the game's RNG
static int roll_3d6(Game& g) {
    return roll_d(g, 6) + roll_d(g, 6) + roll_d(g, 6);
}

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Show current abilities
static void print_abilities(const Abilities& a) {
    std::cout << "Current ability scores:\n";
    std::cout << "  STR: " << a.str << "\n";
    std::cout << "  DEX: " << a.dex << "\n";
    std::cout << "  CON: " << a.con << "\n";
    std::cout << "  INT: " << a.intelligence << "\n";
    std::cout << "  WIS: " << a.wis << "\n";
    std::cout << "  CHA: " << a.cha << "\n\n";
}

Entity create_fighter_hero(Game& g) {
    Entity hero;
    hero.type       = EntityType::Hero;
    hero.char_class = CharacterClass::Fighter;

    std::cout << "=== Character Creation ===\n\n";

    // Name
    std::cout << "Enter your hero's name: ";
    std::string name;
    std::getline(std::cin, name);
    if (name.empty()) {
        name = "Unnamed Fighter";
    }
    hero.name = name;

    // Initial abilities
    hero.abilities.str          = roll_3d6(g);
    hero.abilities.dex          = roll_3d6(g);
    hero.abilities.con          = roll_3d6(g);
    hero.abilities.intelligence = roll_3d6(g);
    hero.abilities.wis          = roll_3d6(g);
    hero.abilities.cha          = roll_3d6(g);

    // Reroll flags: each ability can be rerolled at most once
    bool rerolled_str = false;
    bool rerolled_dex = false;
    bool rerolled_con = false;
    bool rerolled_int = false;
    bool rerolled_wis = false;
    bool rerolled_cha = false;

    // Reroll loop
    while (true) {
        print_abilities(hero.abilities);
        std::cout << "Would you like to reroll one ability? (y/n): ";
        std::string line;
        std::getline(std::cin, line);
        if (line.empty()) break;
        char c = std::tolower(line[0]);
        if (c != 'y') break;

        std::cout << "Which ability to reroll? "
                     "(str, dex, con, int, wis, cha, or 'done')\n> ";
        std::string choice;
        std::getline(std::cin, choice);
        choice = to_lower(choice);
        if (choice == "done" || choice.empty()) break;

        int* target_stat = nullptr;
        bool* rerolled_flag = nullptr;
        std::string label;

        if (choice == "str") {
            target_stat   = &hero.abilities.str;
            rerolled_flag = &rerolled_str;
            label         = "STR";
        } else if (choice == "dex") {
            target_stat   = &hero.abilities.dex;
            rerolled_flag = &rerolled_dex;
            label         = "DEX";
        } else if (choice == "con") {
            target_stat   = &hero.abilities.con;
            rerolled_flag = &rerolled_con;
            label         = "CON";
        } else if (choice == "int") {
            target_stat   = &hero.abilities.intelligence;
            rerolled_flag = &rerolled_int;
            label         = "INT";
        } else if (choice == "wis") {
            target_stat   = &hero.abilities.wis;
            rerolled_flag = &rerolled_wis;
            label         = "WIS";
        } else if (choice == "cha") {
            target_stat   = &hero.abilities.cha;
            rerolled_flag = &rerolled_cha;
            label         = "CHA";
        } else {
            std::cout << "Unknown choice. Try again.\n\n";
            continue;
        }

        if (*rerolled_flag) {
            std::cout << "You have already rerolled " << label
                      << " once. You cannot reroll it again.\n\n";
            continue;
        }

        int old_val = *target_stat;
        int new_val = roll_3d6(g);

        std::cout << "Rerolling " << label << "...\n";
        std::cout << "  Old: " << old_val << "\n";
        std::cout << "  New: " << new_val << "\n";
        std::cout << "Keep (o)ld or (n)ew? ";
        std::string keep_line;
        std::getline(std::cin, keep_line);
        char kc = keep_line.empty() ? 'o' : std::tolower(keep_line[0]);

        if (kc == 'n') {
            *target_stat = new_val;
            std::cout << label << " set to " << new_val << ".\n\n";
        } else {
            std::cout << label << " remains " << old_val << ".\n\n";
        }

        *rerolled_flag = true;
    }

    // Final ability mods
    int str_mod = ability_modifier(hero.abilities.str);
    int dex_mod = ability_modifier(hero.abilities.dex);
    int con_mod = ability_modifier(hero.abilities.con);

    // Level 1 Fighter:
    // HP: 1d8 + CON mod (min 1)
    int hp_roll = roll_d(g, 8);
    int hp_raw = hp_roll + con_mod;
    hero.max_hp = std::max(1, hp_raw);
    hero.hp     = hero.max_hp;

    // Attack bonus: +1 base for Fighter + STR mod (rough B/X-ish)
    hero.attack_bonus = 1 + str_mod;

    // Damage: 1d8 for Fighter
    hero.damage_die = 8;

    // AC: chain-ish 14 + DEX mod (ascending AC)
    hero.ac = 14 + dex_mod;

    hero.morale           = 0;    // PCs don't use morale
    hero.fleeing          = false;
    hero.alive            = true;
    hero.attacks_per_turn = 1;

    // Position is set by init_game (map logic).

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

