#pragma once

#include "entity.hpp"

struct Game;  // forward declaration

// Create a level-1 hero — name, abilities, class selection, stats.
// Prints a hero sheet and waits for Enter before returning.
Entity create_hero(Game& g);

