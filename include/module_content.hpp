#pragma once

#include "module_manifest.hpp"

#include <filesystem>
#include <map>
#include <string>

struct ModuleMapDefinition {
    std::string map_id;
    int width{};
    int height{};
    int entry_x{};
    int entry_y{};
};

struct ModuleLocation {
    std::string location_id;
    std::string title;
    std::string description_id;
    std::string map_id;
};

struct ModuleContent {
    ModuleManifest manifest;
    std::map<std::string, ModuleLocation> locations;
    std::map<std::string, ModuleMapDefinition> maps;
    std::map<std::string, std::string> descriptions;

    const ModuleLocation* find_location(const std::string& location_id) const;
    const ModuleLocation* entry_location() const;
    const ModuleMapDefinition* find_map(const std::string& map_id) const;
    const std::string* find_description(const std::string& description_id) const;
};

struct ModuleContentResult {
    bool ok{false};
    ModuleContent module;
    std::string error;
};

ModuleContentResult load_module_content(const std::filesystem::path& module_dir);
