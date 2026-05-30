#include "map.hpp"
#include "game.hpp"

#include <cstdlib>
#include <algorithm>

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
    if (t.type == TileType::DoorClosed) return true;
    return false;
}

namespace {

bool rooms_overlap(const Room& a, const Room& b, int margin) {
    return a.x - margin < b.x + b.w &&
           a.x + a.w + margin > b.x &&
           a.y - margin < b.y + b.h &&
           a.y + a.h + margin > b.y;
}

void carve_room(Game& g, const Room& r) {
    for (int y = r.y; y < r.y + r.h; ++y) {
        for (int x = r.x; x < r.x + r.w; ++x) {
            g.map[index_for(g, x, y)].type = TileType::Floor;
        }
    }
}

void carve_h_tunnel(Game& g, int x1, int x2, int y) {
    int start_x = std::min(x1, x2);
    int end_x   = std::max(x1, x2);
    for (int x = start_x; x <= end_x; ++x) {
        if (in_bounds(g, x, y)) {
            g.map[index_for(g, x, y)].type = TileType::Floor;
        }
    }
}

void carve_v_tunnel(Game& g, int y1, int y2, int x) {
    int start_y = std::min(y1, y2);
    int end_y   = std::max(y1, y2);
    for (int y = start_y; y <= end_y; ++y) {
        if (in_bounds(g, x, y)) {
            g.map[index_for(g, x, y)].type = TileType::Floor;
        }
    }
}

int rand_range(Game& g, int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(g.rng);
}

// Helper: check if a tile is inside any room
bool is_inside_any_room(int x, int y, const std::vector<Room>& rooms) {
    for (const auto& r : rooms) {
        if (x >= r.x && x < r.x + r.w &&
            y >= r.y && y < r.y + r.h) {
            return true;
        }
    }
    return false;
}

// If the tile is a floor corridor tile (not inside a room), make it a door.
// Returns true if a door was placed.
bool try_place_door(Game& g, int x, int y, const std::vector<Room>& rooms) {
    if (!in_bounds(g, x, y)) return false;
    if (g.map[index_for(g, x, y)].type != TileType::Floor) return false;
    if (is_inside_any_room(x, y, rooms)) return false;
    g.map[index_for(g, x, y)].type = TileType::DoorClosed;
    return true;
}

} // namespace

std::vector<Room> init_map(Game& g) {
    g.map.assign(static_cast<std::size_t>(g.width * g.height),
                 Tile{TileType::Wall});

    std::vector<Room> rooms;

    int num_rooms = rand_range(g, 4, 8);

    for (int attempt = 0; attempt < 50 && static_cast<int>(rooms.size()) < num_rooms; ++attempt) {
        int rw = rand_range(g, 4, 10);
        int rh = rand_range(g, 4, 6);
        int rx = rand_range(g, 1, g.width  - rw - 1);
        int ry = rand_range(g, 1, g.height - rh - 1);

        Room candidate{rx, ry, rw, rh};

        bool overlap = false;
        for (const auto& e : rooms) {
            if (rooms_overlap(candidate, e, 1)) {
                overlap = true;
                break;
            }
        }
        if (overlap) continue;

        carve_room(g, candidate);
        rooms.push_back(candidate);
    }

    if (rooms.empty()) {
        // Fallback: one guaranteed room
        Room fallback{2, 2, 6, 5};
        carve_room(g, fallback);
        rooms.push_back(fallback);
    }

    // Connect rooms sequentially with L-shaped corridors
    for (std::size_t i = 1; i < rooms.size(); ++i) {
        int ax = rooms[i - 1].x + rooms[i - 1].w / 2;
        int ay = rooms[i - 1].y + rooms[i - 1].h / 2;
        int bx = rooms[i].x     + rooms[i].w / 2;
        int by = rooms[i].y     + rooms[i].h / 2;

        if (rand_range(g, 0, 1) == 0) {
            carve_h_tunnel(g, ax, bx, ay);
            carve_v_tunnel(g, ay, by, bx);
        } else {
            carve_v_tunnel(g, ay, by, ax);
            carve_h_tunnel(g, ax, bx, by);
        }
    }

    // Place at most ONE door per room. Scan each side from the center
    // outward and stop after the first successfully placed door.
    for (const auto& r : rooms) {
        bool placed = false;
        // Top side
        for (int dx = 0; !placed && dx * 2 <= r.w; ++dx) {
            int x1 = r.x + r.w / 2 - dx;
            int x2 = r.x + r.w / 2 + dx;
            if (x1 >= r.x) placed = try_place_door(g, x1, r.y - 1, rooms);
            if (!placed && x2 != x1 && x2 < r.x + r.w)
                placed = try_place_door(g, x2, r.y - 1, rooms);
        }
        // Bottom side
        for (int dx = 0; !placed && dx * 2 <= r.w; ++dx) {
            int x1 = r.x + r.w / 2 - dx;
            int x2 = r.x + r.w / 2 + dx;
            if (x1 >= r.x) placed = try_place_door(g, x1, r.y + r.h, rooms);
            if (!placed && x2 != x1 && x2 < r.x + r.w)
                placed = try_place_door(g, x2, r.y + r.h, rooms);
        }
        // Left side
        for (int dy = 0; !placed && dy * 2 <= r.h; ++dy) {
            int y1 = r.y + r.h / 2 - dy;
            int y2 = r.y + r.h / 2 + dy;
            if (y1 >= r.y) placed = try_place_door(g, r.x - 1, y1, rooms);
            if (!placed && y2 != y1 && y2 < r.y + r.h)
                placed = try_place_door(g, r.x - 1, y2, rooms);
        }
        // Right side
        for (int dy = 0; !placed && dy * 2 <= r.h; ++dy) {
            int y1 = r.y + r.h / 2 - dy;
            int y2 = r.y + r.h / 2 + dy;
            if (y1 >= r.y) placed = try_place_door(g, r.x + r.w, y1, rooms);
            if (!placed && y2 != y1 && y2 < r.y + r.h)
                placed = try_place_door(g, r.x + r.w, y2, rooms);
        }
    }

    return rooms;
}
