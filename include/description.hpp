#pragma once

#include <string>
#include <unordered_map>

class DescriptionStore {
public:
    // Load "id = text" lines from a file.
    bool load_from_file(const std::string& path);

    // Return description for id; if missing, return a fallback message.
    std::string get(const std::string& id) const;

private:
    std::unordered_map<std::string, std::string> map_;
};

