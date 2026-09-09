#include "char_session.hpp"

#include "chargen.hpp"
#include "game.hpp"
#include "bx_data.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string format_purse(int gp) {
    int pp = gp / 5;
    int rem_gp = gp % 5;
    std::string s = std::to_string(gp) + " gp";
    if (pp > 0) {
        s += " (" + std::to_string(pp) + " pp";
        if (rem_gp > 0) s += ", " + std::to_string(rem_gp) + " gp";
        s += ")";
    }
    return s;
}

std::string price_status(int cost_gp, int purse_gp) {
    std::string s = std::to_string(cost_gp) + " gp";
    if (purse_gp >= cost_gp) s += " afford";
    else s += " need " + std::to_string(cost_gp - purse_gp) + " gp";
    return s;
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Show current abilities
void print_abilities(const Abilities& a) {
    std::cout << "Current ability scores:\n";
    std::cout << "  STR: " << a.str << "\n";
    std::cout << "  DEX: " << a.dex << "\n";
    std::cout << "  CON: " << a.con << "\n";
    std::cout << "  INT: " << a.intelligence << "\n";
    std::cout << "  WIS: " << a.wis << "\n";
    std::cout << "  CHA: " << a.cha << "\n\n";
}

void show_class_info(const Abilities& a, CharacterClass cc) {
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

// Helper: parse comma/space-separated numbers from a line
std::vector<int> parse_numbers(const std::string& line) {
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
}

// Ask for a 1..count menu choice; reprompts until valid. Returns 1-based index.
int prompt_menu_choice(const std::string& prompt, int count) {
    while (true) {
        std::cout << prompt;
        std::string line;
        std::getline(std::cin, line);
        if (line.empty()) continue;
        int idx = std::atoi(line.c_str());
        if (idx >= 1 && idx <= count) return idx;
    }
}

} // namespace

Entity create_hero_interactive(Game& g) {
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
    hero.abilities = roll_abilities(g);

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

    for (int i = 1; i <= character_class_count(); ++i) {
        std::cout << i << ". ";
        show_class_info(hero.abilities, character_class_at(i));
    }

    CharacterClass chosen = CharacterClass::Fighter;
    while (true) {
        std::cout << "Enter choice (1-" << character_class_count() << "): ";
        std::string line;
        std::getline(std::cin, line);
        if (line.empty()) continue;
        int ci = line[0] - '0';
        if (ci >= 1 && ci <= character_class_count()) {
            chosen = character_class_at(ci);
            break;
        }
    }
    hero.char_class = chosen;

    // Build hero stats based on class using B/X data tables
    int con_mod = constitution_hp_modifier(hero.abilities);
    int dex_mod = dexterity_ac_modifier(hero.abilities);
    int str_mod = strength_to_hit_modifier(hero.abilities);

    hero.max_hp = roll_starting_hp(g, chosen, con_mod);
    hero.hp     = hero.max_hp;

    hero.attack_bonus = attack_bonus_base(chosen) + str_mod;
    hero.damage_die   = damage_die(chosen);
    hero.damage_bonus = str_mod;
    hero.ac           = base_ac(chosen) + dex_mod;

    // --- Equipment selection ---

    int purse_gp = roll_starting_gold(g);
    std::cout << "\nStarting gold: " << format_purse(purse_gp) << "\n";

    // Weapon selection
    std::vector<int> weapon_choices = class_weapon_indices(chosen);
    if (!weapon_choices.empty()) {
        std::cout << "\nChoose your weapon:\n";
        for (std::size_t i = 0; i < weapon_choices.size(); ++i) {
            const auto& w = bx::weapons[weapon_choices[i]];
            std::cout << "  " << (i + 1) << ". " << w.name
                      << " (" << w.damage_die << ")"
                      << " [" << price_status(w.cost_gp, purse_gp) << "]"
                      << (w.two_handed ? " (2H)" : "")
                      << "\n";
        }
        while (true) {
            int idx = prompt_menu_choice("Purse: " + format_purse(purse_gp) +
                                             "\nEnter choice (1-" +
                                             std::to_string(weapon_choices.size()) + "): ",
                                         static_cast<int>(weapon_choices.size()));
            const auto& chosen_w = bx::weapons[weapon_choices[static_cast<std::size_t>(idx - 1)]];
            if (!buy_starting_weapon(hero, weapon_choices[static_cast<std::size_t>(idx - 1)],
                                     purse_gp, str_mod)) {
                std::cout << "You can't afford " << chosen_w.name << ".\n";
                continue;
            }
            std::cout << "Armed with " << chosen_w.name
                      << ". Purse: " << format_purse(purse_gp) << "\n";
            break;
        }
    }

    // Armor selection
    {
        std::vector<int> armor_choices = class_armor_options(chosen);

        std::cout << "\nChoose your armor:\n";
        std::cout << "  1. None (AC base)\n";
        for (std::size_t i = 1; i < armor_choices.size(); ++i) {
            int ai = armor_choices[i] - 1;
            const auto& a = bx::armor_items[static_cast<std::size_t>(ai)];
            std::cout << "  " << (i + 1) << ". " << a.name
                      << " (AC " << a.ac << ")"
                      << " [" << price_status(a.cost_gp, purse_gp) << "]\n";
        }
        while (true) {
            int idx = prompt_menu_choice("Purse: " + format_purse(purse_gp) +
                                             "\nEnter choice (1-" +
                                             std::to_string(armor_choices.size()) + "): ",
                                         static_cast<int>(armor_choices.size()));
            if (idx == 1) {
                hero.armor_id = "none";
                hero.ac = base_ac(chosen) + dex_mod;
            } else {
                int ai = armor_choices[static_cast<std::size_t>(idx - 1)] - 1;
                const auto& chosen_a = bx::armor_items[static_cast<std::size_t>(ai)];
                if (!buy_starting_armor(hero, ai, purse_gp, dex_mod)) {
                    std::cout << "You can't afford " << chosen_a.name << ".\n";
                    continue;
                }
            }
            std::cout << "Purse: " << format_purse(purse_gp) << "\n";
            break;
        }

        // Shield?
        if (class_can_use_shield(chosen)) {
            std::cout << "Use a shield? [" << price_status(10, purse_gp) << "] (y/n): ";
            std::string shield_line;
            std::getline(std::cin, shield_line);
            if (!shield_line.empty() && (shield_line[0] == 'y' || shield_line[0] == 'Y')) {
                if (buy_starting_shield(hero, purse_gp)) {
                    std::cout << "Shield equipped. Purse: " << format_purse(purse_gp) << "\n";
                } else {
                    std::cout << "You can't afford a shield.\n";
                }
            }
        }
    }

    finalize_level_1_hero(hero, purse_gp);

    // Starting spells for spellcasters
    if (chosen == CharacterClass::MagicUser || chosen == CharacterClass::Elf) {
        const auto& l1_spells = class_starting_spells(chosen);
        int n = static_cast<int>(l1_spells.size());
        std::cout << "\nChoose your 1st-level spells (comma/space-separated, e.g. 6,8):\n";
        for (int i = 0; i < n; ++i)
            std::cout << "  " << (i + 1) << ". " << l1_spells[static_cast<std::size_t>(i)] << "\n";
        while (true) {
            std::cout << "Enter choice(s) (1-" << n << "): ";
            std::string line;
            std::getline(std::cin, line);
            if (line.empty()) continue;
            auto nums = parse_numbers(line);
            if (nums.empty()) continue;
            bool all_ok = true;
            for (int v : nums) {
                if (v < 1 || v > n) { all_ok = false; break; }
            }
            if (!all_ok) continue;
            grant_starting_spells(hero, chosen, nums, /*select_all=*/false);
            break;
        }
    }
    if (chosen == CharacterClass::Cleric) {
        const auto& cleric_spells = class_starting_spells(chosen);
        int n = static_cast<int>(cleric_spells.size());
        std::cout << "\nChoose your prepared spells (comma/space-separated, e.g. 1,3):\n";
        for (int i = 0; i < n; ++i)
            std::cout << "  " << (i + 1) << ". "
                      << cleric_spells[static_cast<std::size_t>(i)] << "\n";
        std::cout << "  Enter 'all' for all 7, or just 0 for none.\n";
        while (true) {
            std::cout << "Enter choice(s) (1-" << n << "): ";
            std::string line;
            std::getline(std::cin, line);
            if (line.empty()) continue;
            if (line == "all") {
                grant_starting_spells(hero, chosen, {}, /*select_all=*/true);
                break;
            }
            auto nums = parse_numbers(line);
            if (nums.empty()) continue;
            bool all_ok = true;
            for (int v : nums) {
                if (v < 1 || v > n) { all_ok = false; break; }
            }
            if (!all_ok) continue;
            grant_starting_spells(hero, chosen, nums, /*select_all=*/false);
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
    // Look up weapon name from ID
    {
        std::string wname = "None";
        for (const auto& w : bx::weapons)
            if (hero.weapon_id == w.id) { wname = w.name; break; }
        std::cout << "Weapon: " << wname << " (1d" << hero.damage_die << ")\n";
    }
    {
        std::string aname = "None";
        for (const auto& a : bx::armor_items)
            if (hero.armor_id == a.id) { aname = a.name; break; }
        std::cout << "Armor: " << aname
                  << (hero.has_shield ? " + Shield" : "") << "\n";
    }
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

    std::cout << "Press Enter to begin your adventure...";
    std::string dummy;
    std::getline(std::cin, dummy);

    return hero;
}