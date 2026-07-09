#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Forward declaration; full definition is in game.hpp
struct Game;

enum class TileType {
    Floor,
    Wall,
    DoorClosed,
    DoorOpen,
    StairsDown,
    StairsUp
};

enum class ItemType {
    Torch,
    ScrollArcane,
    ScrollClerical,
    Gold,
    Gem
};

enum class TrapType {
    None,
    Pit,
    Dart,
    PoisonDart
};

struct Tile {
    TileType type = TileType::Floor;
    bool held{false};   // magically held (Hold Portal)
    bool locked{false}; // locked (non-magical, can be picked by Thief)
    bool spiked{false}; // held open/closed by iron spikes
    bool has_trap{false};
    bool trap_visible{false}; // revealed by Search
    TrapType trap_type{TrapType::None};
    bool explored{false};
    bool visible{false};
    // Items lying on this tile (only on visible/explored tiles)
    std::vector<ItemType> items;
    // Spell name on any scroll item on this tile (index matches items[])
    std::vector<std::string> scroll_spells;
};

struct Room {
    int x{}, y{}, w{}, h{};
};

// Map helpers that operate on Game (width/height/map)
std::size_t index_for(const Game& g, int x, int y);
bool in_bounds(const Game& g, int x, int y);
bool is_blocked(const Game& g, int x, int y);

// Procedural dungeon generation. Returns the list of rooms carved.
std::vector<Room> init_map(Game& g);
