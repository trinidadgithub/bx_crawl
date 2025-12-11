#pragma once

#include "entity.hpp"

struct Game;  // forward declaration

// Create a level-1 Fighter hero, asking the user for name
// and rolling abilities. Also prints a hero sheet and waits
// for Enter before returning.
Entity create_fighter_hero(Game& g);

