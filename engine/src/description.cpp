#include "description.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace {

inline void trim_in_place(std::string& s) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };

    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(),
            s.end());
}

} // namespace

bool DescriptionStore::load_from_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }

    std::string current_key;
    std::string current_val;
    bool in_value = false;

    auto flush = [&]() {
        if (!current_key.empty()) {
            trim_in_place(current_val);
            map_[current_key] = current_val;
        }
        current_key.clear();
        current_val.clear();
        in_value = false;
    };

    std::string line;
    while (std::getline(in, line)) {
        // Strip comments
        auto hash_pos = line.find('#');
        if (hash_pos != std::string::npos) {
            line.erase(hash_pos);
        }

        // Trim leading/trailing whitespace for key detection
        std::string trimmed = line;
        trim_in_place(trimmed);
        if (trimmed.empty()) {
            flush();
            continue;
        }

        auto eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            // Check if text before '=' looks like a key (alphanumeric, hyphens, underscores)
            std::string before_eq = line.substr(0, eq_pos);
            trim_in_place(before_eq);
            bool looks_like_key = !before_eq.empty()
                && std::isalpha(static_cast<unsigned char>(before_eq.front()))
                && std::all_of(before_eq.begin(), before_eq.end(), [](char c) {
                    return std::isalnum(static_cast<unsigned char>(c))
                        || c == '-' || c == '_';
                });
            if (looks_like_key) {
                // New key=value line
                flush();
                current_key = before_eq;
                current_val = line.substr(eq_pos + 1);
                in_value = true;
            } else if (in_value) {
                // Continuation line with '=' in the value text
                if (!current_val.empty()) current_val += ' ';
                current_val += trimmed;
            }
        } else if (in_value) {
            // Continuation line — append to current value
            if (!current_val.empty()) current_val += ' ';
            current_val += trimmed;
        }
    }
    flush();

    return true;
}

std::string DescriptionStore::get(const std::string& id) const {
    auto it = map_.find(id);
    if (it != map_.end()) {
        return it->second;
    }
    return std::string{"(No description for '" + id + "')"};
}

bool DescriptionStore::has(const std::string& id) const {
    return map_.find(id) != map_.end();
}

std::string monster_name_to_key(const std::string& name) {
    std::string key;
    for (std::size_t i = 0; i < name.size(); ++i) {
        char c = name[i];
        if (c == ',') {
            key += '-';
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!key.empty() && key.back() != '-')
                key += '-';
            continue;
        }
        if (c == '\'' || c == '(' || c == ')' || c == '"') {
            continue;
        }
        key += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    // Remove trailing dash
    while (!key.empty() && key.back() == '-')
        key.pop_back();
    return key;
}

std::string monster_description(const DescriptionStore& store, const std::string& name) {
    // Try normalized key first: "Dragon, Red" -> "dragon-red"
    auto key = monster_name_to_key(name);
    if (store.has(key))
        return store.get(key);
    // Fall back to raw name
    if (store.has(name))
        return store.get(name);
    return std::string{"(No description for " + name + ")"};
}

