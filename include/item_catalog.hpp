#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

// Validated content-index catalog for the core rules tables.
//
// Inventory counts live in fixed arrays on Entity (weapon_counts / armor_counts /
// gear_counts) whose positions line up with the constexpr tables in bx_data.hpp,
// and monsters/spells are keyed by name. This catalog is the single validated
// mapping from a stable item content id or monster/spell name to its canonical
// table position, and every ID-based operation is validated against it.
// Unknown ids / names yield nullopt, so module-defined content can register
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

// Case-insensitive monster name -> position in bx::monsters. Returns nullopt
// for unknown names.
std::optional<std::size_t> monster_index(std::string_view name) noexcept;

// Case-insensitive spell name -> position in bx::spells (matches the first
// entry that has that name, preserving bx_data iteration order).
std::optional<std::size_t> spell_index(std::string_view name) noexcept;

// Catalog-backed spell rules: the spell's level (0 if unknown) and whether a
// named spell belongs to a given class.
int spell_level_by_name(std::string_view name) noexcept;
bool is_class_spell(std::string_view class_name, std::string_view spell_name) noexcept;

}  // namespace bx