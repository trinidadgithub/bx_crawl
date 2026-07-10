#include "bx_data.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

namespace {

struct ParsedHD {
    int hd{};
    int bonus{};
    bool fractional{};
};

ParsedHD parse_hit_dice(const char* hit_dice) {
    ParsedHD parsed{1, 0, false};
    if (!hit_dice || !*hit_dice) return parsed;

    std::string s(hit_dice);
    if (s.find("point") != std::string::npos || s.find("1/2") != std::string::npos) {
        return ParsedHD{0, 0, true};
    }

    auto star = s.find('*');
    if (star != std::string::npos) s.erase(star);
    auto paren = s.find('(');
    if (paren != std::string::npos) s.erase(paren);

    auto plus = s.find('+');
    if (plus == std::string::npos) plus = s.find('-');
    if (plus != std::string::npos) {
        parsed.hd = std::atoi(s.c_str());
        parsed.bonus = std::atoi(s.c_str() + plus + 1);
        if (s[plus] == '-') parsed.bonus = -parsed.bonus;
    } else {
        parsed.hd = std::atoi(s.c_str());
    }

    if (parsed.hd < 0) parsed.hd = 0;
    return parsed;
}

int challenge_level_for(const bx::Monster& monster) {
    auto hd = parse_hit_dice(monster.hit_dice);
    if (hd.fractional || hd.hd <= 0) return 1;
    if (hd.bonus > 0) return hd.hd + 1;
    return std::max(1, hd.hd);
}

std::string flags_for(const bx::Monster& monster) {
    std::string text;
    auto add = [&](std::string_view flag) {
        if (!text.empty()) text += ',';
        text += flag;
    };

    std::string haystack = std::string(monster.name) + " " + monster.attacks + " " + monster.damage;
    std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (haystack.find("poison") != std::string::npos) add("poison");
    if (haystack.find("paralysis") != std::string::npos) add("paralysis");
    if (haystack.find("breath") != std::string::npos) add("breath");
    if (haystack.find("energy drain") != std::string::npos) add("energy-drain");
    if (haystack.find("str drain") != std::string::npos) add("str-drain");
    if (haystack.find("disease") != std::string::npos) add("disease");
    if (haystack.find("corrosion") != std::string::npos || haystack.find("rust") != std::string::npos) add("gear-damage");
    if (haystack.find("charm") != std::string::npos || haystack.find("song") != std::string::npos) add("charm");
    if (haystack.find("petr") != std::string::npos || haystack.find("medusa") != std::string::npos) add("petrify");

    return text;
}

bool is_valid_monster(const bx::Monster& monster) {
    return monster.name && *monster.name && monster.morale > 0
        && std::string_view(monster.name).find("NPC Party") == std::string_view::npos;
}

void usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [--level N] [--safe] [--all]\n"
              << "\n"
              << "Lists B/X monsters for focused BX_TEST_MONSTER playtesting.\n"
              << "Indexes are 1-based and can be passed to BX_TEST_MONSTER.\n"
              << "\n"
              << "  --level N  show only monsters at exact challenge level N\n"
              << "  --safe     hide monsters with severe special attack flags\n"
              << "  --all      show all valid monsters\n";
}

} // namespace

int main(int argc, char** argv) {
    int level = 1;
    bool show_all = false;
    bool safe_only = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        }
        if (arg == "--all") {
            show_all = true;
        } else if (arg == "--safe") {
            safe_only = true;
        } else if (arg == "--level" && i + 1 < argc) {
            level = std::max(1, std::atoi(argv[++i]));
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            usage(argv[0]);
            return 1;
        }
    }

    std::cout << "Idx  Lvl  Monster                         HD          AC  Damage        Flags\n"
              << "---  ---  ------------------------------  ----------  --  ------------  -----\n";

    for (std::size_t i = 0; i < bx::monsters.size(); ++i) {
        const auto& monster = bx::monsters[i];
        if (!is_valid_monster(monster)) continue;

        int challenge_level = challenge_level_for(monster);
        auto flags = flags_for(monster);
        if (safe_only && !flags.empty()) continue;
        if (!show_all && challenge_level != level) continue;

        std::cout << std::right << std::setw(3) << (i + 1) << "  "
                  << std::setw(3) << challenge_level << "  "
                  << std::left << std::setw(30) << monster.name << "  "
                  << std::setw(10) << monster.hit_dice << "  "
                  << std::right << std::setw(2) << monster.ac << "  "
                  << std::left << std::setw(12) << monster.damage << "  "
                  << flags << "\n";
    }

    return 0;
}
