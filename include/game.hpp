#pragma once

#include <random>
#include <vector>
#include <string>
#include "entity.hpp"
#include "map.hpp"
#include "description.hpp"

struct Game {
    int width{};
    int height{};
    std::vector<Tile> map;
    Entity hero;
    std::vector<Entity> monsters;
    std::mt19937 rng;

    DescriptionStore descriptions;
    std::string current_location_id;
};

// Random helper
int roll_d(Game& g, int sides);

// Game lifecycle / logic
void init_game(Game& g);
void render(const Game& g);
bool handle_player_input(Game& g);
void monsters_turn(Game& g);

// Utility: are there any monsters still alive?
bool any_monsters_alive(const Game& g);

