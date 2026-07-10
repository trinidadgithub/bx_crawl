#pragma once

#include <string>
#include <unordered_map>

class DescriptionStore {
public:
    // Load "id = text" lines from a file.
    bool load_from_file(const std::string& path);

    // Return description for id; if missing, return a fallback message.
    std::string get(const std::string& id) const;

    // Check if a key exists
    bool has(const std::string& id) const;

private:
    std::unordered_map<std::string, std::string> map_;
};

// Normalize a monster display name to a description key lookup.
// "Dragon, Red" -> "dragon-red", "Ape, White" -> "ape-white"
std::string monster_name_to_key(const std::string& name);

// Look up a monster description: first try the normalized key,
// then fall back to the raw name (e.g. "Acolyte").
std::string monster_description(const DescriptionStore& store, const std::string& name);

