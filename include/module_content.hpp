#pragma once

#include "map.hpp"
#include "module_manifest.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct ModuleMapDefinition {
    std::string map_id;
    int width{};
    int height{};
    int entry_x{};
    int entry_y{};
    std::vector<TileType> tiles;

    const TileType* tile_at(int x, int y) const;
};

struct ModuleDoorDefinition {
    std::string door_id;
    std::string from_location_id;
    std::string to_location_id;
    int x{};
    int y{};
    bool locked{false};
};

struct ModuleEncounter {
    std::string encounter_id;
    std::string monster_content_id;
    int x{};
    int y{};
    int count{1};
};

struct ModuleTreasure {
    std::string treasure_id;
    // "gold" or "gem" — maps onto the core tile ItemType pile rules.
    std::string type;
    int x{};
    int y{};
    int count{1};
};

struct ModuleTrap {
    std::string trap_id;
    // "pit", "dart", or "poison_dart" — maps onto TrapType.
    std::string type;
    int x{};
    int y{};
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
    std::map<std::string, ModuleDoorDefinition> doors;
    std::vector<ModuleEncounter> encounters;
    std::vector<ModuleTreasure> treasures;
    std::vector<ModuleTrap> traps;
    std::map<std::string, std::string> descriptions;

    const ModuleLocation* find_location(const std::string& location_id) const;
    const ModuleLocation* entry_location() const;
    const ModuleMapDefinition* find_map(const std::string& map_id) const;
    const ModuleDoorDefinition* find_door(const std::string& door_id) const;
    const std::string* find_description(const std::string& description_id) const;
};

struct ModuleContentResult {
    bool ok{false};
    ModuleContent module;
    std::string error;
};

ModuleContentResult load_module_content(const std::filesystem::path& module_dir);
