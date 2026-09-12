#include "item_catalog.hpp"

#include "bx_data.hpp"
#include "entity.hpp"

#include <cctype>
#include <tuple>

namespace {

bool case_insensitive_equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

}  // namespace

// The fixed inventory arrays on Entity are the storage backing for the bx_data
// tables. Pin the sizes together so a future bx_data table change fails the
// build here instead of silently desyncing the counts arrays.
static_assert(bx::weapons.size() ==
              std::tuple_size_v<decltype(std::declval<Entity>().weapon_counts)>,
              "Entity::weapon_counts size must match bx::weapons table size");
static_assert(bx::armor_items.size() ==
              std::tuple_size_v<decltype(std::declval<Entity>().armor_counts)>,
              "Entity::armor_counts size must match bx::armor_items table size");
static_assert(bx::gear_items.size() ==
              std::tuple_size_v<decltype(std::declval<Entity>().gear_counts)>,
              "Entity::gear_counts size must match bx::gear_items table size");

namespace bx {

std::optional<std::size_t> weapon_index(std::string_view id) noexcept {
    for (std::size_t i = 0; i < weapons.size(); ++i)
        if (std::string_view(weapons[i].id) == id) return i;
    return std::nullopt;
}

std::optional<std::size_t> armor_index(std::string_view id) noexcept {
    for (std::size_t i = 0; i < armor_items.size(); ++i)
        if (std::string_view(armor_items[i].id) == id) return i;
    return std::nullopt;
}

std::optional<std::size_t> gear_index(std::string_view id) noexcept {
    for (std::size_t i = 0; i < gear_items.size(); ++i)
        if (std::string_view(gear_items[i].id) == id) return i;
    return std::nullopt;
}

std::optional<InventorySlot> inventory_item_by_id(std::string_view id) noexcept {
    if (auto i = weapon_index(id)) return InventorySlot{InventorySlotKind::Weapon, *i};
    if (auto i = armor_index(id)) return InventorySlot{InventorySlotKind::Armor, *i};
    if (auto i = gear_index(id)) return InventorySlot{InventorySlotKind::Gear, *i};
    return std::nullopt;
}

std::optional<std::size_t> monster_index(std::string_view name) noexcept {
    for (std::size_t i = 0; i < monsters.size(); ++i)
        if (case_insensitive_equal(std::string_view(monsters[i].name), name)) return i;
    return std::nullopt;
}

std::optional<std::size_t> spell_index(std::string_view name) noexcept {
    for (std::size_t i = 0; i < spells.size(); ++i) {
        std::string_view candidate(spells[i].name);
        if (case_insensitive_equal(candidate, name)) return i;
    }
    return std::nullopt;
}

int spell_level_by_name(std::string_view name) noexcept {
    if (auto i = spell_index(name)) return spells[*i].level;
    return 0;
}

bool is_class_spell(std::string_view class_name, std::string_view spell_name) noexcept {
    if (auto i = spell_index(spell_name))
        return case_insensitive_equal(std::string_view(spells[*i].class_name), class_name);
    return false;
}

}  // namespace bx