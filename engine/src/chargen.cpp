#include "chargen.hpp"
#include "game.hpp"
#include "bx_data.hpp"
#include "item_catalog.hpp"
#include "game_internal.hpp"

#include <algorithm>
#include <string>
#include <string_view>

// Roll 3d6 using the game's RNG
int roll_3d6(Game& g) {
    return roll_d(g, 6) + roll_d(g, 6) + roll_d(g, 6);
}

Abilities roll_abilities(Game& g) {
    Abilities a;
    a.str          = roll_3d6(g);
    a.dex          = roll_3d6(g);
    a.con          = roll_3d6(g);
    a.intelligence = roll_3d6(g);
    a.wis          = roll_3d6(g);
    a.cha          = roll_3d6(g);
    return a;
}

int character_class_count() {
    return 7;
}

CharacterClass character_class_at(int index) {
    switch (index) {
        case 1: return CharacterClass::Fighter;
        case 2: return CharacterClass::MagicUser;
        case 3: return CharacterClass::Thief;
        case 4: return CharacterClass::Cleric;
        case 5: return CharacterClass::Dwarf;
        case 6: return CharacterClass::Elf;
        case 7: return CharacterClass::Halfling;
        default: return CharacterClass::None;
    }
}

int hit_die_sides(CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return 8;
        case CharacterClass::Cleric:    return 6;
        case CharacterClass::Thief:     return 4;
        case CharacterClass::MagicUser: return 4;
        case CharacterClass::Dwarf:     return 8;
        case CharacterClass::Elf:       return 6;
        case CharacterClass::Halfling:  return 6;
        default:                        return 8;
    }
}

int damage_die(CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return 8;
        case CharacterClass::Cleric:    return 6;
        case CharacterClass::Thief:     return 4;
        case CharacterClass::MagicUser: return 4;
        case CharacterClass::Dwarf:     return 8;
        case CharacterClass::Elf:       return 6;
        case CharacterClass::Halfling:  return 6;
        default:                        return 8;
    }
}

int base_ac(CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return 14;
        case CharacterClass::Cleric:    return 14;
        case CharacterClass::Thief:     return 12;
        case CharacterClass::MagicUser: return 10;
        case CharacterClass::Dwarf:     return 14;
        case CharacterClass::Elf:       return 12;
        case CharacterClass::Halfling:  return 12;
        default:                        return 14;
    }
}

int attack_bonus_base(CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return 0;
        case CharacterClass::Dwarf:     return 1;
        default:                        return 0;
    }
}

int prime_requisite_score(const Abilities& a, CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return a.str;
        case CharacterClass::MagicUser: return a.intelligence;
        case CharacterClass::Thief:     return a.dex;
        case CharacterClass::Cleric:    return a.wis;
        case CharacterClass::Dwarf:     return a.str;
        case CharacterClass::Elf:       return a.intelligence;
        case CharacterClass::Halfling:  return a.dex;
        default:                        return a.str;
    }
}

const char* prime_name(CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return "STR";
        case CharacterClass::MagicUser: return "INT";
        case CharacterClass::Thief:     return "DEX";
        case CharacterClass::Cleric:    return "WIS";
        case CharacterClass::Dwarf:     return "STR";
        case CharacterClass::Elf:       return "INT";
        case CharacterClass::Halfling:  return "DEX";
        default:                        return "STR";
    }
}

// Prime-requisite XP bonus: 13-15 → +5%, 16-17 → +10%, 18 → +15%
int xp_bonus(int prime_score) {
    if (prime_score >= 18) return 15;
    if (prime_score >= 16) return 10;
    if (prime_score >= 13) return 5;
    return 0;
}

int strength_to_hit_modifier(const Abilities& a) {
    return bx::get_strength(a.str).toHitMelee;
}

int dexterity_ac_modifier(const Abilities& a) {
    // DEX AC bonus is descending AC (negative = bonus); negate for ascending AC
    return -bx::get_dexterity(a.dex).acBonus;
}

int constitution_hp_modifier(const Abilities& a) {
    return bx::get_constitution(a.con).hpBonusPerDie;
}

int roll_starting_gold(Game& g) {
    return roll_3d6(g) * 10;
}

int roll_starting_hp(Game& g, CharacterClass cc, int con_mod) {
    int hp_raw = roll_d(g, hit_die_sides(cc)) + con_mod;
    return std::max(1, hp_raw);
}

std::vector<int> class_weapon_indices(CharacterClass cc) {
    std::vector<int> result;
    for (int i = 0; i < static_cast<int>(bx::weapons.size()); ++i) {
        const auto& w = bx::weapons[static_cast<std::size_t>(i)];
        if (cc == CharacterClass::Fighter || cc == CharacterClass::Dwarf ||
            cc == CharacterClass::Elf || cc == CharacterClass::Halfling) {
            result.push_back(i);
        } else if (cc == CharacterClass::Cleric) {
            if (w.cleric_allowed) result.push_back(i);
        } else if (cc == CharacterClass::Thief) {
            std::string id(w.id);
            if (id == "dagger" || id == "short-sword" || id == "sling")
                result.push_back(i);
        } else if (cc == CharacterClass::MagicUser) {
            if (std::string_view(w.id) == "dagger") result.push_back(i);
        }
    }
    return result;
}

std::vector<int> class_armor_options(CharacterClass cc) {
    std::vector<int> result;
    result.push_back(0); // "None"
    if (game_internal::can_wear_heavy_armor(cc)) {
        for (int i = 0; i < static_cast<int>(bx::armor_items.size()); ++i)
            result.push_back(i + 1); // +1 because index 0 = None
    } else {
        // Thieves & Magic-Users: leather only (armor_items[0] = leather)
        result.push_back(1);
    }
    return result;
}

bool class_can_use_shield(CharacterClass cc) {
    return game_internal::can_use_shield(cc);
}

bool buy_starting_weapon(Entity& hero, int bx_weapon_index, int& purse_gp, int str_mod) {
    if (bx_weapon_index < 0 ||
        bx_weapon_index >= static_cast<int>(bx::weapons.size()))
        return false;
    const auto& w = bx::weapons[static_cast<std::size_t>(bx_weapon_index)];
    if (purse_gp < w.cost_gp) return false;
    purse_gp -= w.cost_gp;
    hero.weapon_id = w.id;
    hero.weapon_counts[static_cast<std::size_t>(bx_weapon_index)]++;
    hero.damage_die = game_internal::parse_die_size(w.damage_die);
    hero.damage_bonus = str_mod;
    return true;
}

bool buy_starting_armor(Entity& hero, int bx_armor_index, int& purse_gp, int dex_mod) {
    if (bx_armor_index < 0 ||
        bx_armor_index >= static_cast<int>(bx::armor_items.size()))
        return false;
    const auto& a = bx::armor_items[static_cast<std::size_t>(bx_armor_index)];
    if (purse_gp < a.cost_gp) return false;
    purse_gp -= a.cost_gp;
    hero.armor_id = a.id;
    hero.armor_counts[static_cast<std::size_t>(bx_armor_index)]++;
    hero.ac = a.ac + dex_mod;
    return true;
}

bool buy_starting_shield(Entity& hero, int& purse_gp) {
    const auto shield_slot = bx::armor_index("shield");
    if (!shield_slot) return false;
    const auto& shield = bx::armor_items[*shield_slot];
    if (purse_gp < shield.cost_gp) return false;
    purse_gp -= shield.cost_gp;
    hero.has_shield = true;
    hero.armor_counts[*shield_slot]++;
    hero.ac += shield.ac;   // B/X: shield gives a -1 AC bonus
    return true;
}

const std::vector<std::string>& class_starting_spells(CharacterClass cc) {
    static const std::vector<std::string> magic_user_spells = {
        "Charm Person", "Detect Magic", "Floating Disc",
        "Hold Portal", "Light", "Magic Missile",
        "Protection from Evil", "Shield", "Sleep", "Ventriloquism"
    };
    static const std::vector<std::string> cleric_spells = {
        "Cure Light Wounds", "Detect Evil", "Light",
        "Protection from Evil", "Purify Food and Water",
        "Remove Fear", "Resist Cold"
    };
    static const std::vector<std::string> empty;
    if (cc == CharacterClass::MagicUser || cc == CharacterClass::Elf)
        return magic_user_spells;
    if (cc == CharacterClass::Cleric) return cleric_spells;
    return empty;
}

void grant_starting_spells(Entity& hero, CharacterClass cc,
                           const std::vector<int>& selected_indices, bool select_all) {
    if (cc != CharacterClass::MagicUser && cc != CharacterClass::Elf &&
        cc != CharacterClass::Cleric)
        return;

    hero.spell_slots[1] = 1;       // one 1st-level slot
    hero.max_spell_slots[1] = 1;

    if (cc == CharacterClass::MagicUser || cc == CharacterClass::Elf)
        hero.spells.push_back("Read Magic");

    const auto& list = class_starting_spells(cc);
    if (select_all) {
        for (const auto& s : list) hero.spells.push_back(s);
        return;
    }

    std::vector<int> idx = selected_indices;
    std::sort(idx.begin(), idx.end());
    idx.erase(std::unique(idx.begin(), idx.end()), idx.end());
    for (int i : idx) {
        if (i >= 1 && i <= static_cast<int>(list.size()))
            hero.spells.push_back(list[static_cast<std::size_t>(i - 1)]);
    }
}

void finalize_level_1_hero(Entity& hero, int gold_pieces) {
    hero.gold_pieces = gold_pieces;
    hero.morale           = 0;
    hero.fleeing          = false;
    hero.alive            = true;
    hero.attacks_per_turn = 1;

    // Ranged ammunition
    hero.knives = 3;
    hero.darts  = 3;

    // Thief skills (B/X level 1)
    if (hero.char_class == CharacterClass::Thief) {
        hero.skill_hide        = 10;
        hero.skill_pick_locks  = 15;
        hero.skill_find_traps  = 10;
        hero.skill_climb       = 85;
        hero.skill_move_silent = 20;
        hero.skill_hear_noise  = 33; // approx: 1-2 on 1d6
    }
}