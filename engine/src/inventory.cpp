#include "game.hpp"
#include "game_internal.hpp"
#include "bx_data.hpp"

#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

void log_event(Game& g, const std::string& msg) {
    game_internal::emit_log_event(g, msg);
}

bool current_weapon_two_handed(const Entity& hero) {
    for (const auto& w : bx::weapons)
        if (w.id == hero.weapon_id) return w.two_handed;
    return false;
}

} // namespace

// Parse a B/X damage die string like "1d8" → die size (e.g. 8)
namespace game_internal {

int parse_die_size(const char* s) {
    if (!s || !*s) return 2;
    std::string_view sv(s);
    auto d = sv.find('d');
    if (d == std::string_view::npos) return 2;
    return std::atoi(sv.data() + d + 1);
}

bool can_use_shield(CharacterClass cc) {
    for (const auto& c : bx::class_info) {
        CharacterClass check_cc = CharacterClass::None;
        if (std::string_view(c.id) == "fighter") check_cc = CharacterClass::Fighter;
        else if (std::string_view(c.id) == "magic-user") check_cc = CharacterClass::MagicUser;
        else if (std::string_view(c.id) == "thief") check_cc = CharacterClass::Thief;
        else if (std::string_view(c.id) == "cleric") check_cc = CharacterClass::Cleric;
        else if (std::string_view(c.id) == "dwarf") check_cc = CharacterClass::Dwarf;
        else if (std::string_view(c.id) == "elf") check_cc = CharacterClass::Elf;
        else if (std::string_view(c.id) == "halfling") check_cc = CharacterClass::Halfling;
        if (check_cc == cc) return c.allowed_shield;
    }
    return false;
}

bool can_wear_heavy_armor(CharacterClass cc) {
    for (const auto& c : bx::class_info) {
        CharacterClass check_cc = CharacterClass::None;
        if (std::string_view(c.id) == "fighter") check_cc = CharacterClass::Fighter;
        else if (std::string_view(c.id) == "magic-user") check_cc = CharacterClass::MagicUser;
        else if (std::string_view(c.id) == "thief") check_cc = CharacterClass::Thief;
        else if (std::string_view(c.id) == "cleric") check_cc = CharacterClass::Cleric;
        else if (std::string_view(c.id) == "dwarf") check_cc = CharacterClass::Dwarf;
        else if (std::string_view(c.id) == "elf") check_cc = CharacterClass::Elf;
        else if (std::string_view(c.id) == "halfling") check_cc = CharacterClass::Halfling;
        if (check_cc == cc) return c.allowed_armor_any;
    }
    return false;
}

void equip_owned_weapon(Game& g, int inventory_index) {
    int shown = 0;
    for (std::size_t wi = 0; wi < bx::weapons.size(); ++wi) {
        if (g.hero.weapon_counts[wi] <= 0) continue;
        if (shown == inventory_index) {
            const auto& w = bx::weapons[wi];
            g.hero.weapon_id = w.id;
            g.hero.damage_die = parse_die_size(w.damage_die);
            if (w.two_handed) g.hero.has_shield = false;
            log_event(g, std::string("Equipped ") + w.name + ".");
            return;
        }
        ++shown;
    }
    log_event(g, "No such owned weapon.");
}

void equip_owned_armor(Game& g, int armor_index) {
    if (armor_index == -1) {
        g.hero.armor_id.clear();
        g.hero.ac = 9;
        log_event(g, "Unequipped armor.");
        return;
    }
    if (armor_index < 0 || armor_index >= static_cast<int>(bx::armor_items.size())) {
        log_event(g, "No such armor.");
        return;
    }
    if (g.hero.armor_counts[static_cast<std::size_t>(armor_index)] <= 0) {
        log_event(g, "You do not own that armor.");
        return;
    }
    const auto& a = bx::armor_items[static_cast<std::size_t>(armor_index)];
    if (std::string_view(a.id) == "shield") {
        if (g.hero.weapon_id.empty()) {
            log_event(g, "You need a weapon to use a shield.");
        } else if (current_weapon_two_handed(g.hero)) {
            log_event(g, "You can't use a shield with a two-handed weapon.");
        } else {
            g.hero.has_shield = true;
            log_event(g, "Equipped shield.");
        }
        return;
    }
    g.hero.armor_id = a.id;
    g.hero.ac = a.ac;
    log_event(g, std::string("Equipped ") + a.name + ".");
}

} // namespace game_internal

// Check if a weapon is allowed for a given class using bx_data::class_info.
bool is_weapon_allowed(CharacterClass cc, const char* weapon_id) {
    const bx::ClassInfo* ci = nullptr;
    for (const auto& c : bx::class_info) {
        CharacterClass check_cc = CharacterClass::None;
        if (std::string_view(c.id) == "fighter") check_cc = CharacterClass::Fighter;
        else if (std::string_view(c.id) == "magic-user") check_cc = CharacterClass::MagicUser;
        else if (std::string_view(c.id) == "thief") check_cc = CharacterClass::Thief;
        else if (std::string_view(c.id) == "cleric") check_cc = CharacterClass::Cleric;
        else if (std::string_view(c.id) == "dwarf") check_cc = CharacterClass::Dwarf;
        else if (std::string_view(c.id) == "elf") check_cc = CharacterClass::Elf;
        else if (std::string_view(c.id) == "halfling") check_cc = CharacterClass::Halfling;
        if (check_cc == cc) { ci = &c; break; }
    }
    if (!ci) return false;
    std::string_view allowed(ci->allowed_weapons);
    if (allowed == "any") return true;
    // Build a simplified version without hyphens
    std::string simp;
    for (const char* p = weapon_id; *p; ++p)
        if (*p != '-') simp += *p;
    // Tokenize allowed_weapons by commas and compare case-insensitively
    std::string token;
    for (char ch : allowed) {
        if (ch == ',') {
            // Trim and compare
            while (!token.empty() && token.back() == ' ') token.pop_back();
            while (!token.empty() && token[0] == ' ') token.erase(token.begin());
            if (token.size() == simp.size()) {
                bool match = true;
                for (std::size_t i = 0; i < token.size(); ++i)
                    if (std::tolower(static_cast<unsigned char>(token[i])) !=
                        std::tolower(static_cast<unsigned char>(simp[i]))) {
                        match = false; break;
                    }
                if (match) return true;
            }
            token.clear();
        } else {
            token += ch;
        }
    }
    // Last token
    while (!token.empty() && token.back() == ' ') token.pop_back();
    while (!token.empty() && token[0] == ' ') token.erase(token.begin());
    if (token.size() == simp.size()) {
        bool match = true;
        for (std::size_t i = 0; i < token.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(token[i])) !=
                std::tolower(static_cast<unsigned char>(simp[i]))) {
                match = false; break;
            }
        if (match) return true;
    }
    return false;
}