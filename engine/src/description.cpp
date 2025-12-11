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

    std::string line;
    while (std::getline(in, line)) {
        // Strip comments
        auto hash_pos = line.find('#');
        if (hash_pos != std::string::npos) {
            line.erase(hash_pos);
        }

        // Trim
        trim_in_place(line);
        if (line.empty()) continue;

        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            // Malformed line; skip for now
            continue;
        }

        std::string key = line.substr(0, eq_pos);
        std::string val = line.substr(eq_pos + 1);

        trim_in_place(key);
        trim_in_place(val);

        if (!key.empty()) {
            map_[key] = val;
        }
    }

    return true;
}

std::string DescriptionStore::get(const std::string& id) const {
    auto it = map_.find(id);
    if (it != map_.end()) {
        return it->second;
    }
    return std::string{"(No description for '" + id + "')"};
}

