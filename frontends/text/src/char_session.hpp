#pragma once

#include "entity.hpp"

struct Game;  // forward declaration

// Interactive, text-menu character creation. Prompts and output go to
// std::cin/std::cout; the game's RNG supplies the generated numbers.
// Returns a complete level-1 hero that is not yet attached to a Game.
Entity create_hero_interactive(Game& g);