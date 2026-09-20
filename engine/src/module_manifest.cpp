#include "module_manifest.hpp"

#include "content_id.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::size_t kMaxManifestBytes = 64 * 1024;

ModuleManifestResult fail(std::string error) {
    ModuleManifestResult result;
    result.error = std::move(error);
    return result;
}

bool valid_module_id(std::string_view id) {
    if (id.empty())
        return false;
    for (char ch : id) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_'))
            return false;
    }
    return true;
}

bool safe_relative_file_path(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute())
        return false;
    for (const auto& part : path) {
        if (part == "." || part == ".." || part.empty())
            return false;
    }
    return true;
}

std::string read_text_file(const std::filesystem::path& path, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "could not open module.json";
        return {};
    }

    std::ostringstream oss;
    oss << in.rdbuf();
    std::string text = oss.str();
    if (text.size() > kMaxManifestBytes) {
        error = "module.json is too large";
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

bool extract_files(const std::string& json,
                   std::map<std::string, std::filesystem::path>& out) {
    std::regex object_pattern("\"files\"\\s*:\\s*\\{([^}]*)\\}");
    std::smatch object_match;
    if (!std::regex_search(json, object_match, object_pattern))
        return false;

    const std::string body = object_match[1].str();
    std::regex pair_pattern("\"([^\"]+)\"\\s*:\\s*\"([^\"]+)\"");
    for (std::sregex_iterator it(body.begin(), body.end(), pair_pattern), end; it != end; ++it) {
        out[(*it)[1].str()] = std::filesystem::path{(*it)[2].str()};
    }
    return !out.empty();
}

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

ModuleManifestResult validate_manifest(ModuleManifest manifest) {
    if (!valid_module_id(manifest.module_id))
        return fail("invalid module_id");
    if (manifest.title.empty())
        return fail("missing title");
    if (manifest.version.empty())
        return fail("missing version");
    if (manifest.author.empty())
        return fail("missing author");
    if (!is_valid_content_id(manifest.ruleset))
        return fail("invalid ruleset");
    if (manifest.engine_min_version.empty())
        return fail("missing engine_min_version");
    if (manifest.content_schema_version <= 0)
        return fail("invalid content_schema_version");
    if (!is_valid_content_id(manifest.entry_location_id))
        return fail("invalid entry_location_id");

    const std::string module_prefix = "module." + manifest.module_id + ".location.";
    if (!starts_with(manifest.entry_location_id, module_prefix))
        return fail("entry_location_id must belong to module location namespace");

    if (manifest.files.empty())
        return fail("missing files");
    for (const auto& [name, path] : manifest.files) {
        if (!valid_module_id(name))
            return fail("invalid file key");
        if (!safe_relative_file_path(path))
            return fail("unsafe file path");
    }

    ModuleManifestResult result;
    result.ok = true;
    result.manifest = std::move(manifest);
    return result;
}

} // namespace

ModuleManifestResult load_module_manifest(const std::filesystem::path& module_dir) {
    std::error_code ec;
    if (!std::filesystem::is_directory(module_dir, ec))
        return fail("module directory does not exist");

    std::string error;
    std::string json = read_text_file(module_dir / "module.json", error);
    if (!error.empty())
        return fail(error);

    ModuleManifest manifest;
    if (!extract_string(json, "module_id", manifest.module_id))
        return fail("missing module_id");
    if (!extract_string(json, "title", manifest.title))
        return fail("missing title");
    if (!extract_string(json, "version", manifest.version))
        return fail("missing version");
    if (!extract_string(json, "author", manifest.author))
        return fail("missing author");
    if (!extract_string(json, "ruleset", manifest.ruleset))
        return fail("missing ruleset");
    if (!extract_string(json, "engine_min_version", manifest.engine_min_version))
        return fail("missing engine_min_version");
    if (!extract_int(json, "content_schema_version", manifest.content_schema_version))
        return fail("missing content_schema_version");
    if (!extract_string(json, "entry_location_id", manifest.entry_location_id))
        return fail("missing entry_location_id");
    if (!extract_files(json, manifest.files))
        return fail("missing files");

    return validate_manifest(std::move(manifest));
}

std::filesystem::path find_module_dir(const std::filesystem::path& data_dir,
                                      const std::string& module_id) {
    if (module_id.empty())
        return {};
    const auto modules_root = data_dir / "modules";
    std::error_code ec;
    std::filesystem::directory_iterator it(modules_root, ec);
    if (ec)
        return {};
    std::filesystem::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec)
            break;
        const auto& dir = it->path();
        if (!std::filesystem::is_directory(dir, ec) || ec)
            continue;
        ModuleManifestResult result = load_module_manifest(dir);
        if (result.ok && result.manifest.module_id == module_id)
            return dir;
    }
    return {};
}
