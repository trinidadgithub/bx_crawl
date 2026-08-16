#pragma once

#include "bx_data.hpp"
#include "content_id.hpp"

#include <vector>

class RulesCatalog {
public:
    static const RulesCatalog& core();

    const bx::Weapon* find_weapon(ContentId id) const;
    const bx::ArmorItem* find_armor(ContentId id) const;
    const bx::GearItem* find_gear(ContentId id) const;
    const bx::Spell* find_spell(ContentId id) const;
    const bx::Monster* find_monster(ContentId id) const;

    std::vector<ContentId> weapon_ids() const;
    std::vector<ContentId> armor_ids() const;
    std::vector<ContentId> gear_ids() const;
    std::vector<ContentId> spell_ids() const;
    std::vector<ContentId> monster_ids() const;
};

ContentId content_id_for_weapon(const bx::Weapon& weapon);
ContentId content_id_for_armor(const bx::ArmorItem& armor);
ContentId content_id_for_gear(const bx::GearItem& gear);
ContentId content_id_for_spell(const bx::Spell& spell);
ContentId content_id_for_monster(const bx::Monster& monster);
