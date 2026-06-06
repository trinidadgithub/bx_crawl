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


// Prime-requisite XP bonus: 13-15 → +5%, 16-17 → +10%, 18 → +15%
static int xp_bonus(int prime_score) {
    if (prime_score >= 18) return 15;
    if (prime_score >= 16) return 10;
    if (prime_score >= 13) return 5;
    return 0;
}

static int hit_die_sides(CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return 8;
        case CharacterClass::Cleric:    return 6;
        case CharacterClass::Thief:     return 4;
        case CharacterClass::MagicUser: return 4;
        default:                        return 8;
    }
}

static int damage_die(CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return 8;
        case CharacterClass::Cleric:    return 6;
        case CharacterClass::Thief:     return 4;
        case CharacterClass::MagicUser: return 4;
        default:                        return 8;
    }
}

static int base_ac(CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return 14;
        case CharacterClass::Cleric:    return 14;
        case CharacterClass::Thief:     return 12;
        case CharacterClass::MagicUser: return 10;
        default:                        return 14;
    }
}

static int attack_bonus_base(CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return 1;
        default:                        return 0;
    }
}

static int prime_requisite_score(const Abilities& a, CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return a.str;
        case CharacterClass::MagicUser: return a.intelligence;
        case CharacterClass::Thief:     return a.dex;
        case CharacterClass::Cleric:    return a.wis;
        default:                        return a.str;
    }
}

static const char* prime_name(CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return "STR";
        case CharacterClass::MagicUser: return "INT";
        case CharacterClass::Thief:     return "DEX";
        case CharacterClass::Cleric:    return "WIS";
        default:                        return "STR";
    }
}

static void show_class_info(const Abilities& a, CharacterClass cc) {
    int pr = prime_requisite_score(a, cc);
    int bonus = xp_bonus(pr);
    std::cout << "  " << class_name(cc)
              << "  HD: 1d" << hit_die_sides(cc)
              << "  DMG: 1d" << damage_die(cc)
              << "  AC: " << base_ac(cc) << " + DEX\n";
    std::cout << "       Prime: " << prime_name(cc)
              << " (" << pr << ")"
              << (bonus > 0 ? " +" : " ")
              << bonus << "% XP\n\n";
}

// Build ability scores, reroll loop, then let player choose class.
Entity create_hero(Game& g) {
    Entity hero;
    hero.type       = EntityType::Hero;

    std::cout << "=== Character Creation ===\n\n";

    // Name
    std::cout << "Enter your hero's name: ";
    std::string name;
    std::getline(std::cin, name);
    if (name.empty()) {
        name = "Unnamed Adventurer";
    }
    hero.name = name;

    // Initial abilities
    hero.abilities.str          = roll_3d6(g);
    hero.abilities.dex          = roll_3d6(g);
    hero.abilities.con          = roll_3d6(g);
    hero.abilities.intelligence = roll_3d6(g);
    hero.abilities.wis          = roll_3d6(g);
    hero.abilities.cha          = roll_3d6(g);

    // Reroll flags
    bool rerolled_str = false, rerolled_dex = false;
    bool rerolled_con = false, rerolled_int = false;
    bool rerolled_wis = false, rerolled_cha = false;

    // Reroll loop
    while (true) {
        print_abilities(hero.abilities);
        std::cout << "Would you like to reroll one ability? (y/n): ";
        std::string line;
        std::getline(std::cin, line);
        if (line.empty()) break;
        char c = std::tolower(line[0]);
        if (c == 'n') break;
        if (c != 'y') continue; // reprompt on anything else

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

    // Class selection
    std::cout << "\n=== Choose Your Class ===\n\n";

    CharacterClass choices[] = {
        CharacterClass::Fighter,
        CharacterClass::MagicUser,
        CharacterClass::Thief,
        CharacterClass::Cleric
    };

    for (int i = 0; i < 4; ++i) {
        std::cout << i + 1 << ". ";
        show_class_info(hero.abilities, choices[i]);
    }

    CharacterClass chosen = CharacterClass::Fighter;
    while (true) {
        std::cout << "Enter choice (1-4): ";
        std::string line;
        std::getline(std::cin, line);
        if (line.empty()) continue;
        char c = line[0];
        if (c == '1') { chosen = CharacterClass::Fighter; break; }
        if (c == '2') { chosen = CharacterClass::MagicUser; break; }
        if (c == '3') { chosen = CharacterClass::Thief; break; }
        if (c == '4') { chosen = CharacterClass::Cleric; break; }
    }

    hero.char_class = chosen;

    // Build hero stats based on class
    int con_mod = ability_modifier(hero.abilities.con);
    int dex_mod = ability_modifier(hero.abilities.dex);
    int str_mod = ability_modifier(hero.abilities.str);

    int hd = hit_die_sides(chosen);
    int hp_roll = roll_d(g, hd);
    int hp_raw = hp_roll + con_mod;
    hero.max_hp = std::max(1, hp_raw);
    hero.hp     = hero.max_hp;

    hero.attack_bonus = attack_bonus_base(chosen) + str_mod;
    hero.damage_die   = damage_die(chosen);
    hero.damage_bonus = str_mod;
    hero.ac           = base_ac(chosen) + dex_mod;

    hero.morale           = 0;
    hero.fleeing          = false;
    hero.alive            = true;
    hero.attacks_per_turn = 1;

    // Ranged ammunition
    hero.knives = 3;
    hero.darts  = 3;

    // Thief skills (B/X level 1)
    if (chosen == CharacterClass::Thief) {
        hero.skill_hide        = 10;
        hero.skill_pick_locks  = 15;
        hero.skill_find_traps  = 10;
        hero.skill_climb       = 85;
        hero.skill_move_silent = 20;
        hero.skill_hear_noise  = 33; // approx: 1-2 on 1d6
    }

    // Helper: parse comma/space-separated numbers from a line
    auto parse_numbers = [](const std::string& line) -> std::vector<int> {
        std::vector<int> result;
        std::string buf;
        for (char c : line) {
            if (c >= '0' && c <= '9') {
                buf += c;
            } else {
                if (!buf.empty()) {
                    result.push_back(std::stoi(buf));
                    buf.clear();
                }
            }
        }
        if (!buf.empty())
            result.push_back(std::stoi(buf));
        return result;
    };

    // Starting spells for spellcasters
    if (chosen == CharacterClass::MagicUser) {
        hero.spell_slots[1] = 1; // one 1st-level slot
        hero.max_spell_slots[1] = 1;
        hero.spells.push_back("Read Magic");
        const char* l1_spells[] = {
            "Charm Person", "Detect Magic", "Floating Disc",
            "Hold Portal", "Light", "Magic Missile",
            "Protection from Evil", "Shield", "Sleep", "Ventriloquism"
        };
        int n = sizeof(l1_spells) / sizeof(l1_spells[0]);
        std::cout << "\nChoose your 1st-level spells (comma/space-separated, e.g. 6,8):\n";
        for (int i = 0; i < n; ++i)
            std::cout << "  " << (i + 1) << ". " << l1_spells[i] << "\n";
        while (true) {
            std::cout << "Enter choice(s) (1-" << n << "): ";
            std::string line;
            std::getline(std::cin, line);
            if (line.empty()) continue;
            auto nums = parse_numbers(line);
            if (nums.empty()) continue;
            bool all_ok = true;
            std::sort(nums.begin(), nums.end());
            nums.erase(std::unique(nums.begin(), nums.end()), nums.end());
            for (int v : nums) {
                if (v < 1 || v > n) { all_ok = false; break; }
            }
            if (!all_ok) continue;
            for (int v : nums)
                hero.spells.push_back(l1_spells[v - 1]);
            break;
        }
    }
    if (chosen == CharacterClass::Cleric) {
        hero.spell_slots[1] = 1; // one 1st-level slot
        hero.max_spell_slots[1] = 1;
        const char* cleric_spells[] = {
            "Cure Light Wounds", "Detect Evil", "Light",
            "Protection from Evil", "Purify Food and Water",
            "Remove Fear", "Resist Cold"
        };
        int n = sizeof(cleric_spells) / sizeof(cleric_spells[0]);
        std::cout << "\nChoose your prepared spells (comma/space-separated, e.g. 1,3):\n";
        for (int i = 0; i < n; ++i)
            std::cout << "  " << (i + 1) << ". " << cleric_spells[i] << "\n";
        std::cout << "  Enter 'all' for all 7, or just 0 for none.\n";
        while (true) {
            std::cout << "Enter choice(s) (1-" << n << "): ";
            std::string line;
            std::getline(std::cin, line);
            if (line.empty()) continue;
            if (line == "all") {
                for (int i = 0; i < n; ++i)
                    hero.spells.push_back(cleric_spells[i]);
                break;
            }
            auto nums = parse_numbers(line);
            if (nums.empty()) continue;
            bool all_ok = true;
            std::sort(nums.begin(), nums.end());
            nums.erase(std::unique(nums.begin(), nums.end()), nums.end());
            for (int v : nums) {
                if (v < 1 || v > n) { all_ok = false; break; }
            }
            if (!all_ok) continue;
            for (int v : nums)
                hero.spells.push_back(cleric_spells[v - 1]);
            break;
        }
    }

    // Show character sheet
    std::cout << "\n=== Hero Created ===\n";
    std::cout << "Name: " << hero.name << "\n";
    std::cout << "Class: " << class_name(chosen) << " (Level 1)\n\n";

    std::cout << "STR: " << hero.abilities.str
              << " (mod " << str_mod << ")\n";
    std::cout << "DEX: " << hero.abilities.dex
              << " (mod " << dex_mod << ")\n";
    std::cout << "CON: " << hero.abilities.con
              << " (mod " << con_mod << ")\n";
    std::cout << "INT: " << hero.abilities.intelligence << "\n";
    std::cout << "WIS: " << hero.abilities.wis << "\n";
    std::cout << "CHA: " << hero.abilities.cha << "\n\n";

    int pr_score = prime_requisite_score(hero.abilities, chosen);
    int xp_bonus_val = xp_bonus(pr_score);
    std::cout << "HP: " << hero.hp << "/" << hero.max_hp << "\n";
    std::cout << "Attack bonus: " << hero.attack_bonus << "\n";
    std::cout << "Damage: 1d" << hero.damage_die << "\n";
    std::cout << "AC (ascending): " << hero.ac << "\n";
    std::cout << "XP bonus: "
              << (xp_bonus_val > 0 ? "+" : "")
              << xp_bonus_val << "%\n\n";

    if (!hero.spells.empty()) {
        std::cout << "Spells known: ";
        for (std::size_t i = 0; i < hero.spells.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << hero.spells[i];
        }
        std::cout << "\n\n";
    }

    std::cout << "Press Enter to enter the dungeon...";
    std::string dummy;
    std::getline(std::cin, dummy);

    return hero;
}
