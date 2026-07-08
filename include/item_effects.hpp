#pragma once

#include <string_view>

struct Entity;

bool has_gear(const Entity& hero, std::string_view gear_id);
int gear_count(const Entity& hero, std::string_view gear_id);
bool consume_gear(Entity& hero, std::string_view gear_id, int amount = 1);
