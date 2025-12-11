#include "map.hpp"
#include "game.hpp"

int index_for(const Game& g, int x, int y) {
    return y * g.width + x;
}

bool in_bounds(const Game& g, int x, int y) {
    return x >= 0 && x < g.width && y >= 0 && y < g.height;
}

bool is_blocked(const Game& g, int x, int y) {
    if (!in_bounds(g, x, y)) return true;
    const Tile& t = g.map[index_for(g, x, y)];
    if (t.type == TileType::Wall) return true;
    return false;
}

void init_map(Game& g) {
    g.map.resize(g.width * g.height);

    // Simple room: walls around the edges, floor inside.
    for (int y = 0; y < g.height; ++y) {
        for (int x = 0; x < g.width; ++x) {
            Tile& t = g.map[index_for(g, x, y)];
            if (x == 0 || y == 0 || x == g.width - 1 || y == g.height - 1) {
                t.type = TileType::Wall;
            } else {
                t.type = TileType::Floor;
            }
        }
    }
}

