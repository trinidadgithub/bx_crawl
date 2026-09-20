#pragma once

#include <filesystem>
#include <map>
#include <string>

struct ModuleManifest {
    std::string module_id;
    std::string title;
    std::string version;
    std::string author;
    std::string ruleset;
    std::string engine_min_version;
    int content_schema_version{};
    std::string entry_location_id;
    std::map<std::string, std::filesystem::path> files;
};

struct ModuleManifestResult {
    bool ok{false};
    ModuleManifest manifest;
    std::string error;
};

ModuleManifestResult load_module_manifest(const std::filesystem::path& module_dir);

// Locate the directory under `data_dir/modules` whose module.json declares
// `module_id`. Sibling directories are scanned by their declared module id, so
// a directory name never needs to match. Returns an empty path when no module
// with this id is installed.
std::filesystem::path find_module_dir(const std::filesystem::path& data_dir,
                                      const std::string& module_id);
