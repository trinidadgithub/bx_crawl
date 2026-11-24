#pragma once

#include "bx_data.hpp"
#include "entity.hpp"

inline constexpr int ascending_armor_class(int bx_ac) {
    return 19 - bx_ac;
}

inline int dexterity_armor_bonus(const Entity& e) {
    return -bx::get_dexterity(e.abilities.dex).acBonus;
}

inline int unarmored_armor_class(const Entity& e) {
    return ascending_armor_class(9) + dexterity_armor_bonus(e);
}

inline bool weapon_allows_shield(const Entity& e) {
    for (const auto& w : bx::weapons)
        if (e.weapon_id == w.id) return !w.two_handed;
    return true;
}

inline int equipped_armor_class(const Entity& e) {
    return e.ac + (e.has_shield && weapon_allows_shield(e) ? 1 : 0);
}

inline int effective_armor_class(const Entity& e) {
    return equipped_armor_class(e)
        + (e.shield_turns > 0 ? 4 : 0)
        + (e.protection_turns > 0 ? 1 : 0)
        + (e.resist_cold_turns > 0 ? 1 : 0);
}
