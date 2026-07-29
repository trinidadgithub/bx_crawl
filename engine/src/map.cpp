#include "map.hpp"
#include "game.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>

bool in_bounds(const Game& g, int x, int y) {
    return x >= 0 && x < g.width && y >= 0 && y < g.height;
}

std::size_t index_for(const Game& g, int x, int y) {
    assert(in_bounds(g, x, y));
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(g.width)
         + static_cast<std::size_t>(x);
}

bool is_blocked(const Game& g, int x, int y) {
    if (!in_bounds(g, x, y)) return true;
    const Tile& t = g.map[index_for(g, x, y)];
    if (t.type == TileType::Wall) return true;
    if (t.type == TileType::DoorClosed) return true;
    if (t.type == TileType::SecretDoorClosed) return true;
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
    g.map.assign(static_cast<std::size_t>(g.width * g.height), Tile{});
    for (auto& t : g.map) t.type = TileType::Wall;

    std::vector<Room> rooms;

    // Entrance room — always 5×5 near centre so corridors go in all directions
    {
        Room entrance{g.width / 2 - 2, g.height / 2 - 2, 5, 5};
        carve_room(g, entrance);
        rooms.push_back(entrance);
    }

    int num_rooms = 1 + rand_range(g, 4, 7); // 5..8 total with entrance

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

    // Place stairs: up in entrance room, down in a random non-entrance room
    {
        // Stairs up in entrance room (opposite corner from hero spawn)
        Room& entrance = rooms[0];
        int ux = entrance.x + entrance.w - 2;
        int uy = entrance.y + entrance.h - 2;
        g.map[index_for(g, ux, uy)].type = TileType::StairsUp;

        // Stairs down in a random non-entrance room
        if (rooms.size() > 1) {
            int ri = rand_range(g, 1, static_cast<int>(rooms.size()) - 1);
            Room& stair_room = rooms[ri];
            int dx = stair_room.x + rand_range(g, 1, stair_room.w - 2);
            int dy = stair_room.y + rand_range(g, 1, stair_room.h - 2);
            g.map[index_for(g, dx, dy)].type = TileType::StairsDown;
        } else {
            // Fallback: stairs down in entrance if only one room
            int dx = entrance.x + rand_range(g, 1, entrance.w - 2);
            int dy = entrance.y + rand_range(g, 1, entrance.h - 2);
            g.map[index_for(g, dx, dy)].type = TileType::StairsDown;
        }
    }

    // Place treasure in rooms (gold piles + occasional gems)
    for (const auto& r : rooms) {
        // Skip entrance room (rooms[0]) — no starting treasure
        if (&r == &rooms[0]) continue;

        // 50% chance this room has a gold pile
        if (rand_range(g, 0, 1) == 0) {
            int gx = r.x + rand_range(g, 1, r.w - 2);
            int gy = r.y + rand_range(g, 1, r.h - 2);
            g.map[index_for(g, gx, gy)].items.push_back(ItemType::Gold);
        }

        // 20% chance this room has a gem
        if (rand_range(g, 0, 4) == 0) {
            int gx = r.x + rand_range(g, 1, r.w - 2);
            int gy = r.y + rand_range(g, 1, r.h - 2);
            g.map[index_for(g, gx, gy)].items.push_back(ItemType::Gem);
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

    // Lock some doors (approx 30%, skip entrance room door)
    for (auto& t : g.map) {
        if (t.type == TileType::DoorClosed && rand_range(g, 0, 9) < 3) {
            t.locked = true;
        }
    }

    // Secret doors are normal door positions hidden as wall until searched for.
    for (auto& t : g.map) {
        if (t.type == TileType::DoorClosed && rand_range(g, 0, 5) == 0) {
            t.type = TileType::SecretDoorClosed;
            t.locked = false;
            t.held = false;
            t.spiked = false;
            t.secret_revealed = false;
        }
    }

    // Place traps in rooms (skip entrance room) and occasionally in corridors
    for (std::size_t ri = 1; ri < rooms.size(); ++ri) {
        const auto& r = rooms[ri];
        // 25% chance this room has a trap
        if (rand_range(g, 0, 3) == 0) {
            int tx = r.x + rand_range(g, 1, r.w - 2);
            int ty = r.y + rand_range(g, 1, r.h - 2);
            auto& tile = g.map[index_for(g, tx, ty)];
            if (!tile.has_trap) {
                tile.has_trap = true;
                int trap_roll = rand_range(g, 0, 2);
                if (trap_roll == 0) tile.trap_type = TrapType::Pit;
                else if (trap_roll == 1) tile.trap_type = TrapType::Dart;
                else tile.trap_type = TrapType::PoisonDart;
            }
        }
    }
    // Place a few traps in corridor tiles
    int corridor_traps = rand_range(g, 0, 2);
    for (int i = 0; i < corridor_traps; ++i) {
        // Find a non-room floor tile
        for (int attempt = 0; attempt < 50; ++attempt) {
            int x = rand_range(g, 1, g.width - 2);
            int y = rand_range(g, 1, g.height - 2);
            auto& tile = g.map[index_for(g, x, y)];
            if (tile.type == TileType::Floor && !is_inside_any_room(x, y, rooms) && !tile.has_trap) {
                tile.has_trap = true;
                int trap_roll = rand_range(g, 0, 2);
                if (trap_roll == 0) tile.trap_type = TrapType::Pit;
                else if (trap_roll == 1) tile.trap_type = TrapType::Dart;
                else tile.trap_type = TrapType::PoisonDart;
                break;
            }
        }
    }

    return rooms;
}
