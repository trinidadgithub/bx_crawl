#include "game.hpp"
#include "map.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [--seed N] [--draw]\n"
              << "\n"
              << "Generates one dungeon map and reports door/secret-door counts.\n"
              << "Use --draw to render a debug map where S marks secret doors.\n";
}

char debug_glyph(const Tile& tile) {
    switch (tile.type) {
    case TileType::Floor: return '.';
    case TileType::Wall: return '#';
    case TileType::DoorClosed: return '+';
    case TileType::SecretDoorClosed: return 'S';
    case TileType::DoorOpen: return '/';
    case TileType::StairsDown: return '>';
    case TileType::StairsUp: return '<';
    }
    return '?';
}

} // namespace

int main(int argc, char** argv) {
    unsigned seed = 12345u;
    bool draw = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        }
        if (arg == "--seed" && i + 1 < argc) {
            seed = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
            continue;
        }
        if (arg == "--draw") {
            draw = true;
            continue;
        }
        std::cerr << "Unknown argument: " << arg << "\n";
        usage(argv[0]);
        return 1;
    }

    Game g;
    g.width = 40;
    g.height = 20;
    g.rng.seed(seed);
    auto rooms = init_map(g);

    int closed_doors = 0;
    int secret_doors = 0;
    int open_doors = 0;
    int traps = 0;
    int treasure_tiles = 0;

    for (const auto& tile : g.map) {
        if (tile.type == TileType::DoorClosed) ++closed_doors;
        else if (tile.type == TileType::SecretDoorClosed) ++secret_doors;
        else if (tile.type == TileType::DoorOpen) ++open_doors;
        if (tile.has_trap) ++traps;
        if (!tile.items.empty()) ++treasure_tiles;
    }

    std::cout << "Seed: " << seed << "\n"
              << "Rooms: " << rooms.size() << "\n"
              << "Closed doors: " << closed_doors << "\n"
              << "Secret doors: " << secret_doors << "\n"
              << "Open doors: " << open_doors << "\n"
              << "Traps: " << traps << "\n"
              << "Treasure tiles: " << treasure_tiles << "\n";

    if (draw) {
        std::cout << "\nDebug Map\n"
                  << "=========" << "\n";
        for (int y = 0; y < g.height; ++y) {
            for (int x = 0; x < g.width; ++x) {
                std::cout << debug_glyph(g.map[index_for(g, x, y)]);
            }
            std::cout << "\n";
        }
    }

    return 0;
}
