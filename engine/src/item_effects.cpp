#include "item_effects.hpp"

#include "bx_data.hpp"
#include "entity.hpp"

int gear_count(const Entity& hero, std::string_view gear_id) {
    for (std::size_t i = 0; i < bx::gear_items.size(); ++i) {
        if (gear_id == bx::gear_items[i].id) {
            return hero.gear_counts[i];
        }
    }
    return 0;
}

bool has_gear(const Entity& hero, std::string_view gear_id) {
    return gear_count(hero, gear_id) > 0;
}
