#include "rules_catalog.hpp"

#include <cctype>
#include <string>
#include <string_view>
#include <utility>

namespace {

bool is_segment_char(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
}

ContentId make_bx_content_id(std::string_view category, std::string_view local_name) {
    return make_content_id(category, local_name);
}

template <typename Array, typename MakeId>
auto find_by_content_id(const Array& values, ContentId id, MakeId make_id)
    -> const typename Array::value_type* {
    if (!is_valid_content_id(id.value))
        return nullptr;
    for (const auto& value : values) {
        if (make_id(value) == id)
            return &value;
    }
    return nullptr;
}

template <typename Array, typename MakeId>
std::vector<ContentId> ids_for(const Array& values, MakeId make_id) {
    std::vector<ContentId> out;
    out.reserve(values.size());
    for (const auto& value : values)
        out.push_back(make_id(value));
    return out;
}

} // namespace

bool is_valid_content_id(std::string_view id) {
    if (id.empty() || id.front() == '.' || id.back() == '.')
        return false;

    bool previous_dot = false;
    for (char ch : id) {
        if (ch == '.') {
            if (previous_dot)
                return false;
            previous_dot = true;
            continue;
        }
        previous_dot = false;
        if (!is_segment_char(ch))
            return false;
    }
    return true;
}

std::string canonical_content_segment(std::string_view text) {
    std::string out;
    bool pending_separator = false;
    for (unsigned char raw : text) {
        if (std::isalnum(raw)) {
            if (pending_separator && !out.empty())
                out.push_back('_');
            out.push_back(static_cast<char>(std::tolower(raw)));
            pending_separator = false;
        } else {
            pending_separator = true;
        }
    }
    return out;
}

ContentId make_content_id(std::string_view category, std::string_view local_name) {
    std::string category_segment = canonical_content_segment(category);
    std::string local_segment = canonical_content_segment(local_name);
    if (category_segment.empty() || local_segment.empty())
        return {};
    return ContentId{"bx." + category_segment + "." + local_segment};
}

const RulesCatalog& RulesCatalog::core() {
    static const RulesCatalog catalog;
    return catalog;
}

const bx::Weapon* RulesCatalog::find_weapon(ContentId id) const {
    return find_by_content_id(bx::weapons, std::move(id), content_id_for_weapon);
}

const bx::ArmorItem* RulesCatalog::find_armor(ContentId id) const {
    return find_by_content_id(bx::armor_items, std::move(id), content_id_for_armor);
}

const bx::GearItem* RulesCatalog::find_gear(ContentId id) const {
    return find_by_content_id(bx::gear_items, std::move(id), content_id_for_gear);
}

const bx::Spell* RulesCatalog::find_spell(ContentId id) const {
    return find_by_content_id(bx::spells, std::move(id), content_id_for_spell);
}

const bx::Monster* RulesCatalog::find_monster(ContentId id) const {
    return find_by_content_id(bx::monsters, std::move(id), content_id_for_monster);
}

std::vector<ContentId> RulesCatalog::weapon_ids() const {
    return ids_for(bx::weapons, content_id_for_weapon);
}

std::vector<ContentId> RulesCatalog::armor_ids() const {
    return ids_for(bx::armor_items, content_id_for_armor);
}

std::vector<ContentId> RulesCatalog::gear_ids() const {
    return ids_for(bx::gear_items, content_id_for_gear);
}

std::vector<ContentId> RulesCatalog::spell_ids() const {
    return ids_for(bx::spells, content_id_for_spell);
}

std::vector<ContentId> RulesCatalog::monster_ids() const {
    return ids_for(bx::monsters, content_id_for_monster);
}

ContentId content_id_for_weapon(const bx::Weapon& weapon) {
    return make_bx_content_id("weapon", weapon.id);
}

ContentId content_id_for_armor(const bx::ArmorItem& armor) {
    return make_bx_content_id("armor", armor.id);
}

ContentId content_id_for_gear(const bx::GearItem& gear) {
    return make_bx_content_id("gear", gear.id);
}

ContentId content_id_for_spell(const bx::Spell& spell) {
    std::string class_segment = canonical_content_segment(spell.class_name);
    std::string spell_segment = canonical_content_segment(spell.name);
    if (class_segment.empty() || spell_segment.empty())
        return {};
    return ContentId{"bx.spell." + class_segment + "." + spell_segment};
}

ContentId content_id_for_monster(const bx::Monster& monster) {
    std::string local_segment = canonical_content_segment(monster.name);
    if (local_segment.empty())
        return {};

    int occurrence = 0;
    bool found_in_core_table = false;
    for (const auto& candidate : bx::monsters) {
        if (canonical_content_segment(candidate.name) != local_segment)
            continue;
        ++occurrence;
        if (&candidate == &monster) {
            found_in_core_table = true;
            break;
        }
    }

    if (found_in_core_table && occurrence > 1)
        local_segment += "_" + std::to_string(occurrence);
    return ContentId{"bx.monster." + local_segment};
}
