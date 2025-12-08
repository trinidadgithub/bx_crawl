#pragma once

// Forward declaration; full definition is in game.hpp
struct Game;

enum class TileType {
    Floor,
    Wall
};

struct Tile {
    TileType type = TileType::Floor;
};

// Map helpers that operate on Game (width/height/map)
int  index_for(const Game& g, int x, int y);
bool in_bounds(const Game& g, int x, int y);
bool is_blocked(const Game& g, int x, int y);

// Map initialization
void init_map(Game& g);

