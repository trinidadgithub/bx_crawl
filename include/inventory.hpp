#pragma once

#include <string_view>

struct Entity;

// ID-based inventory access over the bx_data tables (see item_catalog.hpp for
// the validated id -> slot mapping). New and module-facing code should use these
// instead of writing count arrays by raw position.
//
// Unknown content ids are rejected: item_count returns 0, give_item /
// set_item_count are no-ops, and consume_owned_item returns false without
// changing anything.

// Number of owned copies of an item (0 if not owned / unknown).
int item_count(const Entity& e, std::string_view item_id);

// Add `amount` owned copies of an item.
void give_item(Entity& e, std::string_view item_id, int amount = 1);

// Set the owned count for an item to exactly `count`.
void set_item_count(Entity& e, std::string_view item_id, int count);

// Remove `amount` owned copies. Returns false (and changes nothing) when the
// item is unknown or fewer than `amount` are owned.
bool consume_owned_item(Entity& e, std::string_view item_id, int amount = 1);