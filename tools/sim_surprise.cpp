#include "game.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [monster-name] [--trials N] [--seed N]\n"
              << "\n"
              << "Simulates default B/X surprise: each side is surprised on 1-2 on 1d6.\n"
              << "The monster name is informational for now; monster-specific modifiers\n"
              << "will be added after the generic lifecycle is stable.\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string monster = "Monster";
    int trials = 1000;
    unsigned seed = 12345u;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        }
        if (arg == "--trials" && i + 1 < argc) {
            trials = std::max(1, std::atoi(argv[++i]));
            continue;
        }
        if (arg == "--seed" && i + 1 < argc) {
            seed = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
            continue;
        }
        monster = argv[i];
    }

    Game g;
    g.rng.seed(seed);

    int hero_only = 0;
    int monster_only = 0;
    int both = 0;
    int neither = 0;

    for (int i = 0; i < trials; ++i) {
        SurpriseResult result = roll_surprise(g);
        if (result.hero_surprised && result.monsters_surprised) ++both;
        else if (result.hero_surprised) ++hero_only;
        else if (result.monsters_surprised) ++monster_only;
        else ++neither;
    }

    std::cout << "Monster: " << monster << "\n"
              << "Hero surprised on: 1-2 on 1d6\n"
              << "Monster surprised on: 1-2 on 1d6\n"
              << "Modifier: none\n\n"
              << "Trials: " << trials << "\n"
              << "Seed: " << seed << "\n"
              << "Hero surprised only:    " << hero_only << "\n"
              << "Monster surprised only: " << monster_only << "\n"
              << "Both surprised:         " << both << "\n"
              << "Neither surprised:      " << neither << "\n";

    return 0;
}
