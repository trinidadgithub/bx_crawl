#include "item_catalog.hpp"

#include "bx_data.hpp"
#include "entity.hpp"

#include <tuple>

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

}  // namespace bx