#include "bx_data.hpp"
#include "description.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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

int compute_xp_value(const char* hit_dice) {
    auto hd = parse_hit_dice(hit_dice);
    if (hd.fractional || hd.hd <= 0) return 5;

    const int tier = hd.bonus > 0 ? hd.hd + 1 : hd.hd;
    switch (tier) {
    case 1: return 10;
    case 2: return 20;
    case 3: return 35;
    case 4: return 75;
    case 5: return 175;
    case 6: return 275;
    case 7: return 450;
    case 8: return 650;
    case 9: return 900;
    case 10: return 900;
    case 11: return 1100;
    default: return 1100 + (tier - 11) * 250;
    }
}

// Average HP for a B/X monster: uses d8 per HD
int average_hp(const char* hit_dice) {
    auto hd = parse_hit_dice(hit_dice);
    if (hd.fractional) return 2;
    if (hd.hd <= 0) return 2;
    // d8 average = 4.5 per die, plus bonus
    return hd.hd * 4 + (hd.hd + 1) / 2 + hd.bonus;
}

void show_stat_block(const bx::Monster& monster, int index,
                     const DescriptionStore& desc,
                     const DescriptionStore& sources) {
    auto hd = parse_hit_dice(monster.hit_dice);

    // Title
    std::cout << "\n" << monster.name << "\n"
              << std::string(std::strlen(monster.name), '=') << "\n";

    auto print_field = [](const char* label, const char* value, int label_w = 16) {
        std::string v(value ? value : "");
        std::cout << "  " << std::left << std::setw(label_w) << label
                  << v << "\n";
    };

    {
        std::ostringstream oss;
        oss << monster.ac;
        print_field("AC:", oss.str().c_str());
    }
    print_field("HD:", monster.hit_dice);

    {
        std::ostringstream hp;
        if (hd.fractional)
            hp << "1-4 hp";
        else
            hp << average_hp(monster.hit_dice) << " hp (average)";
        print_field("HP:", hp.str().c_str());
    }

    {
        std::ostringstream xp;
        xp << compute_xp_value(monster.hit_dice) << " XP";
        print_field("XP:", xp.str().c_str());
    }

    {
        std::ostringstream cl;
        cl << challenge_level_for(monster);
        print_field("CL:", cl.str().c_str());
    }

    print_field("Move:", monster.move);
    print_field("Attacks:", monster.attacks);
    print_field("Damage:", monster.damage);
    print_field("Appearing:", monster.no_appearing);
    print_field("Saves As:", monster.saves_as);

    {
        std::ostringstream m;
        m << monster.morale;
        print_field("Morale:", m.str().c_str());
    }

    print_field("Treasure:", monster.treasure_type);
    print_field("Alignment:", monster.alignment);

    // Special abilities
    std::string flags;
    auto add_flag = [&](std::string_view f) {
        if (!flags.empty()) flags += ", ";
        flags += f;
    };
    std::string hay = std::string(monster.name) + " "
                    + monster.attacks + " " + monster.damage;
    std::transform(hay.begin(), hay.end(), hay.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (hay.find("poison") != std::string::npos) add_flag("Poison");
    if (hay.find("paralysis") != std::string::npos) add_flag("Paralysis");
    if (hay.find("breath") != std::string::npos) add_flag("Breath Weapon");
    if (hay.find("energy drain") != std::string::npos) add_flag("Energy Drain");
    if (hay.find("disease") != std::string::npos) add_flag("Disease");
    if (hay.find("charm") != std::string::npos || hay.find("song") != std::string::npos)
        add_flag("Charm");
    if (hay.find("petr") != std::string::npos || hay.find("medusa") != std::string::npos)
        add_flag("Petrify");

    if (!flags.empty()) {
        std::cout << "  Special:      " << flags << "\n";
    }

    std::string source = monster_description(sources, monster.name);
    if (source.find("No description") == std::string::npos) {
        std::cout << "\nSource:\n"
                  << "  " << source << "\n";
    }

    // Description
    std::string desc_text = monster_description(desc, monster.name);
    if (desc_text.find("No description") == std::string::npos) {
        std::cout << "\n";
        std::string remaining = desc_text;
        while (!remaining.empty()) {
            std::string line;
            if (remaining.size() <= 72) {
                line = remaining;
                remaining.clear();
            } else {
                auto brk = remaining.rfind(' ', 72);
                if (brk == std::string::npos) brk = 72;
                line = remaining.substr(0, brk);
                remaining = remaining.substr(brk + 1);
            }
            std::cout << "  " << line << "\n";
        }
    }

    std::cout << "\n";
}

bool is_valid_monster(const bx::Monster& monster) {
    return monster.name && *monster.name && monster.morale > 0
        && std::string_view(monster.name).find("NPC Party") == std::string_view::npos;
}

void usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " <name or index>\n"
              << "\n"
              << "Shows the B/X stat block for a monster.\n"
              << "Indexes are 1-based (use list_monsters to find them).\n"
              << "Name matches case-insensitive substrings.\n";
}

bool matches(const char* monster_name, const std::string& query) {
    std::string name(monster_name);
    std::transform(name.begin(), name.end(), name.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name.find(query) != std::string::npos;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    // Load monster descriptions and source references
    DescriptionStore desc;
    DescriptionStore sources;
    std::filesystem::path data_dir = std::filesystem::path{"data"};
    auto desc_path = data_dir / "monster_descriptions.txt";
    if (std::filesystem::exists(desc_path)) {
        desc.load_from_file(desc_path.string());
    }
    auto source_path = data_dir / "monster_sources.txt";
    if (std::filesystem::exists(source_path)) {
        sources.load_from_file(source_path.string());
    }

    std::string query(argv[1]);
    bool query_is_index = !query.empty()
        && std::all_of(query.begin(), query.end(), [](char c) { return c >= '0' && c <= '9'; });

    if (query_is_index && std::string_view(argv[1]) == "0") {
        usage(argv[0]);
        return 1;
    }

    if (query_is_index) {
        int idx = std::atoi(argv[1]) - 1;
        if (idx < 0 || idx >= static_cast<int>(bx::monsters.size())) {
            std::cerr << "Index out of range (1-" << bx::monsters.size() << ").\n";
            return 1;
        }
        const auto& m = bx::monsters[static_cast<std::size_t>(idx)];
        if (!is_valid_monster(m)) {
            std::cerr << "No valid monster at index " << (idx + 1) << ".\n";
            return 1;
        }
        show_stat_block(m, idx, desc, sources);
        return 0;
    }

    // Substring name search
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::vector<int> matches_found;
    for (std::size_t i = 0; i < bx::monsters.size(); ++i) {
        if (is_valid_monster(bx::monsters[i]) && matches(bx::monsters[i].name, lower_query)) {
            matches_found.push_back(static_cast<int>(i));
        }
    }

    if (matches_found.empty()) {
        std::cerr << "No monster matching \"" << query << "\".\n";
        return 1;
    }

    if (matches_found.size() > 1) {
        std::cout << "Multiple matches:\n";
        for (int idx : matches_found) {
            std::cout << "  " << (idx + 1) << ". " << bx::monsters[static_cast<std::size_t>(idx)].name << "\n";
        }
        return 1;
    }

    show_stat_block(bx::monsters[static_cast<std::size_t>(matches_found[0])], matches_found[0], desc, sources);
    return 0;
}
