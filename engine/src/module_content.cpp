#include "module_content.hpp"

#include "content_id.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kMaxModuleTextBytes = 256 * 1024;

ModuleContentResult fail(std::string error) {
    ModuleContentResult result;
    result.error = std::move(error);
    return result;
}

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::string read_text_file(const std::filesystem::path& path, const char* label,
                           std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = std::string{"could not open "} + label;
        return {};
    }

    std::ostringstream oss;
    oss << in.rdbuf();
    std::string text = oss.str();
    if (text.size() > kMaxModuleTextBytes) {
        error = std::string{label} + " is too large";
        return {};
    }
    return text;
}

bool extract_string(const std::string& json, const char* key, std::string& out) {
    std::regex pattern(std::string{"\""} + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (!std::regex_search(json, match, pattern))
        return false;
    out = match[1].str();
    return true;
}

bool extract_int(const std::string& json, const char* key, int& out) {
    std::regex pattern(std::string{"\""} + key + "\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern))
        return false;
    out = std::stoi(match[1].str());
    return true;
}

bool extract_bool(const std::string& json, const char* key, bool& out) {
    std::regex pattern(std::string{"\""} + key + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(json, match, pattern))
        return false;
    out = match[1].str() == "true";
    return true;
}

bool extract_string_array(const std::string& json, const char* key, std::vector<std::string>& out) {
    std::regex array_pattern(std::string{"\""} + key + "\"\\s*:\\s*\\[([^\\]]*)\\]");
    std::smatch array_match;
    if (!std::regex_search(json, array_match, array_pattern))
        return false;

    const std::string body = array_match[1].str();
    std::regex string_pattern("\"([^\"]*)\"");
    for (std::sregex_iterator it(body.begin(), body.end(), string_pattern), end; it != end; ++it) {
        out.push_back((*it)[1].str());
    }
    return !out.empty();
}

std::vector<std::string> extract_flat_objects(const std::string& json) {
    std::vector<std::string> objects;
    std::regex object_pattern("\\{([^{}]*)\\}");
    for (std::sregex_iterator it(json.begin(), json.end(), object_pattern), end; it != end; ++it) {
        objects.push_back((*it)[1].str());
    }
    return objects;
}

std::vector<std::string> extract_flat_object_array(const std::string& json, const char* key) {
    std::regex array_pattern(std::string{"\""} + key + "\"\\s*:\\s*\\[([^\\]]*)\\]");
    std::smatch array_match;
    if (!std::regex_search(json, array_match, array_pattern))
        return {};
    return extract_flat_objects(array_match[1].str());
}

bool valid_module_namespace(std::string_view id, const ModuleManifest& manifest,
                            std::string_view category) {
    return is_valid_content_id(id) &&
           starts_with(id, "module." + manifest.module_id + "." + std::string{category} + ".");
}

bool tile_type_from_glyph(char glyph, TileType& out) {
    switch (glyph) {
    case '#': out = TileType::Wall; return true;
    case '.': out = TileType::Floor; return true;
    case '+': out = TileType::DoorClosed; return true;
    case '/': out = TileType::DoorOpen; return true;
    case '<': out = TileType::StairsUp; return true;
    case '>': out = TileType::StairsDown; return true;
    default: return false;
    }
}

bool tile_is_walkable(TileType type) {
    return type == TileType::Floor || type == TileType::DoorOpen || type == TileType::StairsUp ||
           type == TileType::StairsDown;
}

bool tile_is_door(TileType type) {
    return type == TileType::DoorClosed || type == TileType::DoorOpen ||
           type == TileType::SecretDoorClosed;
}

ModuleContentResult load_locations(const std::filesystem::path& module_dir,
                                   ModuleContent module) {
    auto it = module.manifest.files.find("locations");
    if (it == module.manifest.files.end())
        return fail("manifest missing locations file");

    std::string error;
    std::string json = read_text_file(module_dir / it->second, "locations", error);
    if (!error.empty())
        return fail(error);

    for (const std::string& object : extract_flat_objects(json)) {
        ModuleLocation location;
        if (!extract_string(object, "location_id", location.location_id))
            return fail("location missing location_id");
        if (!extract_string(object, "title", location.title))
            return fail("location missing title");
        if (!extract_string(object, "description_id", location.description_id))
            return fail("location missing description_id");
        if (!extract_string(object, "map_id", location.map_id))
            return fail("location missing map_id");

        if (!valid_module_namespace(location.location_id, module.manifest, "location"))
            return fail("invalid location_id");
        if (!valid_module_namespace(location.description_id, module.manifest, "text"))
            return fail("invalid location description_id");
        if (!valid_module_namespace(location.map_id, module.manifest, "map"))
            return fail("invalid location map_id");
        if (module.locations.contains(location.location_id))
            return fail("duplicate location_id");

        module.locations.emplace(location.location_id, std::move(location));
    }

    if (module.locations.empty())
        return fail("locations file has no locations");
    if (!module.locations.contains(module.manifest.entry_location_id))
        return fail("entry location not found");

    ModuleContentResult result;
    result.ok = true;
    result.module = std::move(module);
    return result;
}

ModuleContentResult load_map(const std::filesystem::path& module_dir, ModuleContent module) {
    auto it = module.manifest.files.find("map");
    if (it == module.manifest.files.end())
        return fail("manifest missing map file");

    std::string error;
    std::string json = read_text_file(module_dir / it->second, "map", error);
    if (!error.empty())
        return fail(error);

    ModuleMapDefinition map;
    if (!extract_string(json, "map_id", map.map_id))
        return fail("map missing map_id");
    if (!extract_int(json, "width", map.width))
        return fail("map missing width");
    if (!extract_int(json, "height", map.height))
        return fail("map missing height");
    if (!extract_int(json, "entry_x", map.entry_x))
        return fail("map missing entry_x");
    if (!extract_int(json, "entry_y", map.entry_y))
        return fail("map missing entry_y");
    std::vector<std::string> rows;
    if (!extract_string_array(json, "tiles", rows))
        return fail("map missing tiles");

    if (!valid_module_namespace(map.map_id, module.manifest, "map"))
        return fail("invalid map_id");
    if (map.width <= 0 || map.height <= 0)
        return fail("invalid map dimensions");
    if (map.entry_x < 0 || map.entry_y < 0 || map.entry_x >= map.width || map.entry_y >= map.height)
        return fail("invalid map entry position");
    if (static_cast<int>(rows.size()) != map.height)
        return fail("map tile height mismatch");

    map.tiles.reserve(static_cast<std::size_t>(map.width * map.height));
    for (const std::string& row : rows) {
        if (static_cast<int>(row.size()) != map.width)
            return fail("map tile width mismatch");
        for (char glyph : row) {
            TileType type{};
            if (!tile_type_from_glyph(glyph, type))
                return fail("invalid map tile glyph");
            map.tiles.push_back(type);
        }
    }

    const auto entry_index = static_cast<std::size_t>(map.entry_y * map.width + map.entry_x);
    if (!tile_is_walkable(map.tiles[entry_index]))
        return fail("map entry position is blocked");

    for (const std::string& object : extract_flat_object_array(json, "doors")) {
        ModuleDoorDefinition door;
        if (!extract_string(object, "door_id", door.door_id))
            return fail("door missing door_id");
        if (!extract_string(object, "from_location_id", door.from_location_id))
            return fail("door missing from_location_id");
        if (!extract_string(object, "to_location_id", door.to_location_id))
            return fail("door missing to_location_id");
        if (!extract_int(object, "x", door.x))
            return fail("door missing x");
        if (!extract_int(object, "y", door.y))
            return fail("door missing y");
        (void)extract_bool(object, "locked", door.locked);

        if (!valid_module_namespace(door.door_id, module.manifest, "door"))
            return fail("invalid door_id");
        if (!module.locations.contains(door.from_location_id) ||
            !module.locations.contains(door.to_location_id)) {
            return fail("door references missing location");
        }
        const TileType* door_tile = map.tile_at(door.x, door.y);
        if (door_tile == nullptr)
            return fail("door position is out of bounds");
        if (!tile_is_door(*door_tile))
            return fail("door position is not a door tile");
        if (module.doors.contains(door.door_id))
            return fail("duplicate door_id");

        module.doors.emplace(door.door_id, std::move(door));
    }

    module.maps.emplace(map.map_id, std::move(map));
    for (const auto& [location_id, location] : module.locations) {
        (void)location_id;
        if (!module.maps.contains(location.map_id))
            return fail("location references missing map");
    }

    ModuleContentResult result;
    result.ok = true;
    result.module = std::move(module);
    return result;
}

std::string trim_trailing_newlines(std::string text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        text.pop_back();
    return text;
}

ModuleContentResult load_descriptions(const std::filesystem::path& module_dir,
                                      ModuleContent module) {
    auto it = module.manifest.files.find("descriptions");
    if (it == module.manifest.files.end())
        return fail("manifest missing descriptions file");

    std::string error;
    std::string text = read_text_file(module_dir / it->second, "descriptions", error);
    if (!error.empty())
        return fail(error);

    std::istringstream input(text);
    std::string line;
    std::string current_id;
    std::ostringstream current_text;
    auto flush = [&]() -> bool {
        if (current_id.empty())
            return true;
        if (!valid_module_namespace(current_id, module.manifest, "text"))
            return false;
        module.descriptions[current_id] = trim_trailing_newlines(current_text.str());
        current_id.clear();
        current_text.str({});
        current_text.clear();
        return true;
    };

    while (std::getline(input, line)) {
        if (!line.empty() && line.front() == '[' && line.back() == ']') {
            if (!flush())
                return fail("invalid description_id");
            current_id = line.substr(1, line.size() - 2);
            continue;
        }
        if (!current_id.empty())
            current_text << line << '\n';
    }
    if (!flush())
        return fail("invalid description_id");

    if (module.descriptions.empty())
        return fail("descriptions file has no descriptions");
    for (const auto& [location_id, location] : module.locations) {
        (void)location_id;
        if (!module.descriptions.contains(location.description_id))
            return fail("location references missing description");
    }

    ModuleContentResult result;
    result.ok = true;
    result.module = std::move(module);
    return result;
}

ModuleContentResult load_encounters(const std::filesystem::path& module_dir,
                                    ModuleContent module) {
    auto it = module.manifest.files.find("encounters");
    if (it == module.manifest.files.end()) {
        // Module opts out of scripted encounters.
        ModuleContentResult result;
        result.ok = true;
        result.module = std::move(module);
        return result;
    }

    std::string error;
    std::string json = read_text_file(module_dir / it->second, "encounters", error);
    if (!error.empty())
        return fail(std::move(error));

    const ModuleMapDefinition* entry_map = nullptr;
    const ModuleLocation* entry = module.entry_location();
    if (entry == nullptr)
        return fail("module entry location not found");
    entry_map = module.find_map(entry->map_id);
    if (entry_map == nullptr)
        return fail("encounter entry map not found");

    for (const std::string& object : extract_flat_objects(json)) {
        ModuleEncounter encounter;
        if (!extract_string(object, "encounter_id", encounter.encounter_id))
            return fail("encounter missing encounter_id");
        if (!extract_string(object, "monster", encounter.monster_content_id))
            return fail("encounter missing monster");
        if (!extract_int(object, "x", encounter.x))
            return fail("encounter missing x");
        if (!extract_int(object, "y", encounter.y))
            return fail("encounter missing y");
        (void)extract_int(object, "count", encounter.count);

        if (!valid_module_namespace(encounter.encounter_id, module.manifest, "encounter"))
            return fail("invalid encounter_id");
        if (!is_valid_content_id(encounter.monster_content_id))
            return fail("invalid monster content id");
        if (encounter.count < 1)
            return fail("invalid encounter count");
        for (const ModuleEncounter& existing : module.encounters)
            if (existing.encounter_id == encounter.encounter_id)
                return fail("duplicate encounter_id");

        const TileType* tile = entry_map->tile_at(encounter.x, encounter.y);
        if (tile == nullptr)
            return fail("encounter position is out of bounds");
        if (!tile_is_walkable(*tile))
            return fail("encounter tile is not walkable");

        module.encounters.push_back(std::move(encounter));
    }

    ModuleContentResult result;
    result.ok = true;
    result.module = std::move(module);
    return result;
}

} // namespace

const TileType* ModuleMapDefinition::tile_at(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height)
        return nullptr;
    const auto index = static_cast<std::size_t>(y * width + x);
    if (index >= tiles.size())
        return nullptr;
    return &tiles[index];
}

const ModuleLocation* ModuleContent::find_location(const std::string& location_id) const {
    auto it = locations.find(location_id);
    if (it == locations.end())
        return nullptr;
    return &it->second;
}

const ModuleLocation* ModuleContent::entry_location() const {
    return find_location(manifest.entry_location_id);
}

const ModuleMapDefinition* ModuleContent::find_map(const std::string& map_id) const {
    auto it = maps.find(map_id);
    if (it == maps.end())
        return nullptr;
    return &it->second;
}

const ModuleDoorDefinition* ModuleContent::find_door(const std::string& door_id) const {
    auto it = doors.find(door_id);
    if (it == doors.end())
        return nullptr;
    return &it->second;
}

const std::string* ModuleContent::find_description(const std::string& description_id) const {
    auto it = descriptions.find(description_id);
    if (it == descriptions.end())
        return nullptr;
    return &it->second;
}

ModuleContentResult load_module_content(const std::filesystem::path& module_dir) {
    ModuleManifestResult manifest_result = load_module_manifest(module_dir);
    if (!manifest_result.ok)
        return fail(manifest_result.error);

    ModuleContent module;
    module.manifest = std::move(manifest_result.manifest);

    ModuleContentResult locations = load_locations(module_dir, std::move(module));
    if (!locations.ok)
        return locations;

    ModuleContentResult map = load_map(module_dir, std::move(locations.module));
    if (!map.ok)
        return map;

    ModuleContentResult descriptions = load_descriptions(module_dir, std::move(map.module));
    if (!descriptions.ok)
        return descriptions;

    return load_encounters(module_dir, std::move(descriptions.module));
}
