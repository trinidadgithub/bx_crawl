#pragma once

#include <vector>

// Forward declaration; full definition is in game.hpp
struct Game;

enum class TileType {
    Floor,
    Wall
};

struct Tile {
    TileType type = TileType::Floor;
};

struct Room {
    int x{}, y{}, w{}, h{};
};

// Map helpers that operate on Game (width/height/map)
int  index_for(const Game& g, int x, int y);
bool in_bounds(const Game& g, int x, int y);
bool is_blocked(const Game& g, int x, int y);

// Procedural dungeon generation. Returns the list of rooms carved.
std::vector<Room> init_map(Game& g);
