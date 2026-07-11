#include "bx_data.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

namespace {

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return s;
}

void usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [type]\n"
              << "\n"
              << "Shows B/X lair treasure type mechanics currently present in bx_data.hpp.\n"
              << "Examples:\n"
              << "  " << argv0 << "\n"
              << "  " << argv0 << " A\n";
}

void print_entry(const char* label, const bx::TreasureEntry& e) {
    std::cout << "  " << std::left << std::setw(10) << label;
    if (e.chance_pct <= 0) {
        std::cout << "-\n";
        return;
    }
    std::cout << std::right << std::setw(3) << e.chance_pct << "%  "
              << e.range_str;
    if (e.unit && *e.unit) std::cout << " " << e.unit;
    std::cout << "\n";
}

void print_table(const bx::LairTreasure& t) {
    std::cout << "\nLair Treasure Type " << t.id << "\n"
              << std::string(14 + std::string_view(t.id).size(), '=') << "\n";
    print_entry("Copper:", t.copper);
    print_entry("Silver:", t.silver);
    print_entry("Electrum:", t.electrum);
    print_entry("Gold:", t.gold);
    print_entry("Platinum:", t.platinum);
    print_entry("Gems:", t.gems);
    std::cout << "  " << std::left << std::setw(10) << "Magic:"
              << (t.magic_items && *t.magic_items ? t.magic_items : "-") << "\n"
              << "  " << std::left << std::setw(10) << "Avg GP:"
              << t.avg_value_gp << "\n";
}

void print_table(const bx::IndividualTreasure& t) {
    std::cout << "\nIndividual Treasure Type " << t.id << "\n"
              << std::string(20 + std::string_view(t.id).size(), '=') << "\n";
    print_entry("Copper:", t.copper);
    print_entry("Silver:", t.silver);
    print_entry("Electrum:", t.electrum);
    print_entry("Gold:", t.gold);
    print_entry("Platinum:", t.platinum);
    print_entry("Gems:", t.gems);
    std::cout << "  " << std::left << std::setw(10) << "Magic:"
              << (t.magic_items && *t.magic_items ? t.magic_items : "-") << "\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 2 || (argc == 2 && (std::string_view(argv[1]) == "--help"
        || std::string_view(argv[1]) == "-h"))) {
        usage(argv[0]);
        return argc > 2 ? 1 : 0;
    }

    std::string filter;
    if (argc == 2) filter = upper(argv[1]);

    bool found = false;
    for (const auto& table : bx::lair_treasure_types) {
        if (!filter.empty() && filter != table.id) continue;
        print_table(table);
        found = true;
    }

    for (const auto& table : bx::individual_treasure_types) {
        if (!filter.empty() && filter != table.id) continue;
        print_table(table);
        found = true;
    }

    if (!found) {
        std::cerr << "No treasure type " << filter << " in bx_data.hpp.\n";
        return 1;
    }

    std::cout << "\n* Gems/Jewelry entries with two percentages are Gems / Jewelry.\n";
    return 0;
}
