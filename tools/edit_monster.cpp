#include "bx_data.hpp"
#include "description.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool is_valid_monster(const bx::Monster& monster) {
    return monster.name && *monster.name && monster.morale > 0
        && std::string_view(monster.name).find("NPC Party") == std::string_view::npos;
}

bool matches(const char* monster_name, const std::string& query) {
    std::string name(monster_name);
    std::transform(name.begin(), name.end(), name.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name.find(query) != std::string::npos;
}

void usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " <name or index>\n"
              << "\n"
              << "Edits a monster's description in\n"
              << "data/monster_descriptions.txt\n"
              << "Shows the current description, then reads a new one from\n"
              << "stdin (multiline; Ctrl+D to finish).\n"
              << "Indexes are 1-based (use list_monsters to find them).\n"
              << "Name matches case-insensitive substrings.\n";
}

int find_monster(const std::string& query) {
    // Check if numeric index
    bool is_num = !query.empty()
        && std::all_of(query.begin(), query.end(), [](char c) { return c >= '0' && c <= '9'; });
    if (is_num && query != "0") {
        int idx = std::atoi(query.c_str()) - 1;
        if (idx >= 0 && idx < static_cast<int>(bx::monsters.size())
            && is_valid_monster(bx::monsters[static_cast<std::size_t>(idx)])) {
            return idx;
        }
        return -1;
    }

    // Substring name search
    std::string lower = query;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::vector<int> found;
    for (std::size_t i = 0; i < bx::monsters.size(); ++i) {
        if (is_valid_monster(bx::monsters[i]) && matches(bx::monsters[i].name, lower)) {
            found.push_back(static_cast<int>(i));
        }
    }

    if (found.empty()) return -1;
    if (found.size() == 1) return found[0];

    std::cout << "Multiple matches:\n";
    for (int idx : found) {
        std::cout << "  " << (idx + 1) << ". " << bx::monsters[static_cast<std::size_t>(idx)].name << "\n";
    }
    return -1;
}

std::string normalized_path() {
    std::filesystem::path p = "data/monster_descriptions.txt";
    if (std::filesystem::exists(p)) return p.string();

    std::error_code ec;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        auto exe_dir = exe.parent_path();
        for (const auto& candidate : {exe_dir / "../data/monster_descriptions.txt",
                                      exe_dir / "../../data/monster_descriptions.txt"}) {
            if (std::filesystem::exists(candidate)) {
                auto canonical = std::filesystem::weakly_canonical(candidate, ec);
                return ec ? candidate.string() : canonical.string();
            }
        }
    }

    return "data/monster_descriptions.txt";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    std::string query(argv[1]);
    int idx = find_monster(query);
    if (idx < 0) {
        if (query == "--help" || query == "-h") {
            usage(argv[0]);
            return 0;
        }
        std::cerr << "No monster matching \"" << query << "\".\n";
        return 1;
    }

    const auto& monster = bx::monsters[static_cast<std::size_t>(idx)];
    std::string key = monster_name_to_key(monster.name);

    // Load descriptions
    DescriptionStore desc;
    auto desc_path = normalized_path();
    desc.load_from_file(desc_path);

    std::string current = desc.get(key);
    if (current.find("No description") != std::string::npos) {
        current = monster.name;
        current += " — ";
        current += monster.name;
        current += " are monsters encountered in dungeons and wilderness.\n";
        current += "See the B/X rules for full details.";
    }

    // Show the description and ask what to do
    std::cout << "Current description for " << monster.name << ":\n"
              << "────────────────────────────────────────\n"
              << current << "\n"
              << "────────────────────────────────────────\n"
              << "\n"
              << "Enter new description (Ctrl+D when done, or empty to cancel):\n";

    // Read multiline input from stdin
    std::string new_desc;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!new_desc.empty()) new_desc += '\n';
        new_desc += line;
    }

    if (new_desc.empty()) {
        std::cout << "No changes made.\n";
        return 0;
    }

    // Read the entire file and rewrite with updated entry
    std::ifstream in(desc_path);
    if (!in) {
        std::cerr << "Cannot read " << desc_path << "\n";
        return 1;
    }

    std::vector<std::string> lines;
    std::string fline;
    while (std::getline(in, fline)) {
        lines.push_back(fline);
    }
    in.close();

    // Find the key line and replace the entry
    bool found = false;
    bool in_entry = false;
    std::vector<std::string> new_lines;

    // Build the new entry text
    std::vector<std::string> entry_lines;
    entry_lines.push_back(key + " = " + new_desc);

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto& l = lines[i];

        // Detect start of entry: "key ="
        if (!in_entry
            && (l.find(key + " =") != std::string::npos
                || l.find(key + "=") != std::string::npos)) {
            in_entry = true;
            found = true;
            // Append the new entry
            for (const auto& el : entry_lines) {
                new_lines.push_back(el);
            }
            continue;
        }

        if (in_entry) {
            // Skip continuation lines (no '=' that looks like a new key)
            std::string trimmed = l;
            auto not_space = [](unsigned char c) { return !std::isspace(c); };
            trimmed.erase(trimmed.begin(),
                std::find_if(trimmed.begin(), trimmed.end(), not_space));
            auto eq = trimmed.find('=');
            bool is_new_key = eq != std::string::npos
                && !trimmed.empty()
                && std::isalpha(static_cast<unsigned char>(trimmed.front()))
                && std::all_of(trimmed.begin(), trimmed.begin() + eq,
                    [](char c) { return std::isalnum(static_cast<unsigned char>(c))
                        || c == '-' || c == '_' || c == ' '; });
            if (is_new_key) {
                in_entry = false;
                new_lines.push_back(l);
                continue;
            }
            // Skip continuation line
            continue;
        }

        new_lines.push_back(l);
    }

    if (!found) {
        // Append new entry at end
        if (!new_lines.empty() && !new_lines.back().empty()) {
            new_lines.push_back("");
        }
        for (const auto& el : entry_lines) {
            new_lines.push_back(el);
        }
    }

    // Write back
    std::ofstream out(desc_path);
    if (!out) {
        std::cerr << "Cannot write " << desc_path << "\n";
        return 1;
    }
    for (std::size_t i = 0; i < new_lines.size(); ++i) {
        out << new_lines[i];
        if (i + 1 < new_lines.size()) out << "\n";
    }
    out.close();

    std::cout << "Description updated for " << monster.name << ".\n";
    return 0;
}
