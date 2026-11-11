#pragma once

#include "bx_data.hpp"
#include "entity.hpp"

#include <optional>

namespace bx {

struct SaveBand {
    int first_level;
    int last_level;
    SaveRow saves;
};

// Cook/Marsh Expert (1981), X24. Ranges are table coverage, not class level caps.
inline constexpr SaveBand cleric_save_bands[] = {
    {1, 4, {11, 12, 14, 16, 15}},
    {5, 8, {9, 10, 12, 14, 12}},
    {9, 12, {6, 7, 9, 11, 9}},
    {13, 16, {3, 5, 7, 8, 7}},
};
inline constexpr SaveBand fighter_save_bands[] = {
    {1, 3, {12, 13, 14, 15, 16}},
    {4, 6, {10, 11, 12, 13, 14}},
    {7, 9, {8, 9, 10, 10, 12}},
    {10, 12, {6, 7, 8, 8, 10}},
    {13, 15, {4, 5, 6, 5, 8}},
};
inline constexpr SaveBand magic_user_save_bands[] = {
    {1, 5, {13, 14, 13, 16, 15}},
    {6, 10, {11, 12, 11, 14, 12}},
    {11, 15, {8, 9, 8, 11, 8}},
};
inline constexpr SaveBand thief_save_bands[] = {
    {1, 4, {13, 14, 13, 16, 15}},
    {5, 8, {12, 13, 11, 14, 13}},
    {9, 12, {10, 11, 9, 12, 10}},
    {13, 16, {8, 9, 7, 10, 8}},
};
inline constexpr SaveBand dwarf_save_bands[] = {
    {1, 3, {8, 9, 10, 13, 12}},
    {4, 6, {6, 7, 8, 10, 10}},
    {7, 9, {4, 5, 6, 7, 8}},
    {10, 12, {2, 3, 4, 4, 6}},
};
inline constexpr SaveBand halfling_save_bands[] = {
    {1, 3, {8, 9, 10, 13, 12}},
    {4, 6, {6, 7, 8, 10, 10}},
    {7, 9, {4, 5, 6, 7, 8}},
};
inline constexpr SaveBand elf_save_bands[] = {
    {1, 3, {12, 13, 13, 15, 15}},
    {4, 6, {10, 11, 11, 13, 12}},
    {7, 9, {8, 9, 9, 10, 10}},
    {10, 10, {6, 7, 8, 8, 8}},
};

template <std::size_t N>
constexpr std::optional<SaveRow> save_row_in(const SaveBand (&bands)[N], int level) {
    for (const auto& band : bands)
        if (level >= band.first_level && level <= band.last_level)
            return band.saves;
    return std::nullopt;
}

constexpr std::optional<SaveRow> expert_save_row(CharacterClass cc, int level) {
    switch (cc) {
        case CharacterClass::Cleric: return save_row_in(cleric_save_bands, level);
        case CharacterClass::Fighter: return save_row_in(fighter_save_bands, level);
        case CharacterClass::MagicUser: return save_row_in(magic_user_save_bands, level);
        case CharacterClass::Thief: return save_row_in(thief_save_bands, level);
        case CharacterClass::Dwarf: return save_row_in(dwarf_save_bands, level);
        case CharacterClass::Halfling: return save_row_in(halfling_save_bands, level);
        case CharacterClass::Elf: return save_row_in(elf_save_bands, level);
        case CharacterClass::None:
            if (level == 1) return SaveRow{14, 15, 16, 17, 18};
            return std::nullopt;
    }
    return std::nullopt;
}

} // namespace bx
