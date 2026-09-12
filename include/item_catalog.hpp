#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

// Validated content-index catalog for inventory items.
//
// Inventory counts live in fixed arrays on Entity (weapon_counts / armor_counts /
// gear_counts) whose positions line up with the constexpr tables in bx_data.hpp.
// This catalog is the single mapping from a stable item content id to its
// canonical table position, and every ID-based inventory operation is validated
// against it. Unknown ids yield nullopt, so module-defined items can register
// here later without callers assuming a magic array position.
namespace bx {

enum class InventorySlotKind { Weapon, Armor, Gear };

struct InventorySlot {
    InventorySlotKind kind;
    std::size_t index;  // canonical position in the matching bx_data table
};

// Stable item content id -> position in bx::weapons / bx::armor_items /
// bx::gear_items. Returns nullopt for unknown ids.
std::optional<std::size_t> weapon_index(std::string_view id) noexcept;
std::optional<std::size_t> armor_index(std::string_view id) noexcept;
std::optional<std::size_t> gear_index(std::string_view id) noexcept;

// Unified lookup across all three tables.
std::optional<InventorySlot> inventory_item_by_id(std::string_view id) noexcept;

}  // namespace bx