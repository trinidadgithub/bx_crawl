#include "game.hpp"
#include "item_catalog.hpp"

#include "bx_data.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <istream>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace {

// Hard limit on a single serialized string in a save file. Legitimate saves
// only ever contain short names, item ids, and spell names (tens of
// characters), so this cap is far above real data while preventing a corrupt
// file from triggering a pathological allocation.
constexpr std::size_t kMaxSerializedStringLength = 1024;

// Maximum number of known spells a save may declare. Spellcasters can only
// ever learn a few dozen spell names, even across a long campaign.
constexpr int32_t kMaxSerializedSpellCount = 100;

// Hero level is hard-capped at 14 in progression code.
constexpr int32_t kMaxHeroLevel = 14;

// No code-level cap exists for dungeon depth, but guarding level arithmetic
// (which does `dungeon_level + 1`) against int32 overflow is cheap. 100 is far
// beyond any authored content.
constexpr int32_t kMaxDungeonLevel = 100;

// Byte-exact stream helpers. Every persistence read/write goes through these
// so a short-read or a failed write is never silently ignored.
bool write_exact(std::ostream& out, const char* src, std::size_t n) {
    out.write(src, static_cast<std::streamsize>(n));
    return out.good();
}

bool read_exact(std::istream& in, char* dst, std::size_t n) {
    in.read(dst, static_cast<std::streamsize>(n));
    return static_cast<std::size_t>(in.gcount()) == n;
}

bool write_str(std::ostream& out, const std::string& s) {
    if (s.size() > kMaxSerializedStringLength)
        return false;
    const int32_t len = static_cast<int32_t>(s.size());
    return write_exact(out, reinterpret_cast<const char*>(&len), sizeof(len)) &&
           write_exact(out, s.data(), static_cast<std::size_t>(len));
}

// Read a length-prefixed string. Negative and excessive lengths are rejected
// before any allocation happens, and the payload must be fully present.
bool read_str(std::istream& in, std::string& out) {
    int32_t len = 0;
    if (!read_exact(in, reinterpret_cast<char*>(&len), sizeof(len)))
        return false;
    if (len < 0)
        return false;
    const std::size_t n = static_cast<std::size_t>(len);
    if (n > kMaxSerializedStringLength)
        return false;
    out.assign(n, '\0');
    return read_exact(in, out.data(), n);
}

template <typename T>
bool write_int(std::ostream& out, T v) {
    return write_exact(out, reinterpret_cast<const char*>(&v), sizeof(v));
}

template <typename T>
bool read_int(std::istream& in, T& v) {
    return read_exact(in, reinterpret_cast<char*>(&v), sizeof(v));
}

// Decode the two serialized enums. Values must match the enum declarations in
// game.hpp / entity.hpp; anything else is a corrupt save.
bool decode_game_state(int32_t v, GameState& out) {
    if (v < static_cast<int32_t>(GameState::Town) ||
        v > static_cast<int32_t>(GameState::CharacterSheet))
        return false;
    out = static_cast<GameState>(v);
    return true;
}

bool decode_character_class(int32_t v, CharacterClass& out) {
    if (v < static_cast<int32_t>(CharacterClass::None) ||
        v > static_cast<int32_t>(CharacterClass::Halfling))
        return false;
    out = static_cast<CharacterClass>(v);
    return true;
}

// Deserialize a complete save stream into `out`. `out` is intentionally a
// scratch/temporary Game: the caller only commits it to live state after this
// returns true, so a failed load can never partially mutate the caller's state.
bool read_game_from(std::istream& in, Game& out) {
    // Magic header
    char magic[4];
    if (!read_exact(in, magic, sizeof(magic)))
        return false;
    if (magic[0] != 'B' || magic[1] != 'X' || magic[2] != 'S' || magic[3] != 'V')
        return false;

    int32_t version = 0;
    if (!read_int(in, version))
        return false;
    if (version < 1 || version > 5)
        return false;

    Entity& h = out.hero;

    // Game-level state
    int32_t st = 0;
    if (!read_int(in, st))
        return false;
    if (!decode_game_state(st, out.state))
        return false;
    if (!read_int(in, out.total_gold))
        return false;
    if (!read_int(in, out.total_xp))
        return false;
    if (!read_int(in, out.hero_level))
        return false;
    if (!read_int(in, out.dungeon_level))
        return false;
    if (out.hero_level < 1 || out.hero_level > kMaxHeroLevel)
        return false;
    if (out.dungeon_level < 1 || out.dungeon_level > kMaxDungeonLevel)
        return false;

    // Hero data
    if (!read_str(in, h.name))
        return false;
    if (!read_int(in, h.hp))
        return false;
    if (!read_int(in, h.max_hp))
        return false;
    if (!read_int(in, h.attack_bonus))
        return false;
    if (!read_int(in, h.damage_die))
        return false;
    if (!read_int(in, h.damage_bonus))
        return false;
    if (!read_int(in, h.ac))
        return false;

    if (!read_int(in, h.abilities.str))
        return false;
    if (!read_int(in, h.abilities.intelligence))
        return false;
    if (!read_int(in, h.abilities.wis))
        return false;
    if (!read_int(in, h.abilities.dex))
        return false;
    if (!read_int(in, h.abilities.con))
        return false;
    if (!read_int(in, h.abilities.cha))
        return false;

    int32_t cc = 0;
    if (!read_int(in, cc))
        return false;
    if (!decode_character_class(cc, h.char_class))
        return false;
    if (!read_int(in, h.torch_turns))
        return false;
    if (!read_int(in, h.light_spell_turns))
        return false;
    if (!read_int(in, h.knives))
        return false;
    if (!read_int(in, h.darts))
        return false;
    if (!read_int(in, h.shield_turns))
        return false;
    if (!read_int(in, h.protection_turns))
        return false;
    if (!read_int(in, h.courage_turns))
        return false;
    if (!read_int(in, h.resist_cold_turns))
        return false;

    if (!read_str(in, h.weapon_id))
        return false;
    if (!read_str(in, h.armor_id))
        return false;
    int32_t shield = 0;
    if (!read_int(in, shield))
        return false;
    h.has_shield = (shield != 0);

    if (!read_int(in, h.skill_hide))
        return false;
    if (!read_int(in, h.skill_pick_locks))
        return false;
    if (!read_int(in, h.skill_find_traps))
        return false;
    if (!read_int(in, h.skill_climb))
        return false;
    if (!read_int(in, h.skill_move_silent))
        return false;
    if (!read_int(in, h.skill_hear_noise))
        return false;

    // Known spells
    int32_t nspells = 0;
    if (!read_int(in, nspells))
        return false;
    if (nspells < 0 || nspells > kMaxSerializedSpellCount)
        return false;
    h.spells.clear();
    h.spells.reserve(static_cast<std::size_t>(nspells));
    for (int32_t i = 0; i < nspells; ++i) {
        std::string s;
        if (!read_str(in, s))
            return false;
        h.spells.push_back(std::move(s));
    }

    // Spell slots (including index 0 - unused placeholder)
    for (int i = 0; i <= Entity::MaxSpellLevel; ++i) {
        if (!read_int(in, h.spell_slots[i]))
            return false;
        if (!read_int(in, h.max_spell_slots[i]))
            return false;
    }

    // Version 2+ fields
    if (version >= 2) {
        if (!read_int(in, h.rations))
            return false;
        if (!read_int(in, h.turns_without_food))
            return false;
        if (!read_int(in, h.oil_flasks))
            return false;
        if (!read_int(in, h.holy_water))
            return false;
        if (!read_int(in, h.ai_patrol_x))
            return false;
        if (!read_int(in, h.ai_patrol_y))
            return false;
        if (!read_int(in, h.ai_wander_timer))
            return false;
    } else {
        h.rations = 0;
        h.turns_without_food = 0;
        h.oil_flasks = 0;
        h.holy_water = 0;
        h.ai_patrol_x = 0;
        h.ai_patrol_y = 0;
        h.ai_wander_timer = 0;
    }

    if (version >= 3) {
        for (int& count : h.gear_counts)
            if (!read_int(in, count))
                return false;
    } else {
        h.gear_counts.fill(0);
    }

    if (version >= 4) {
        if (!read_int(in, h.lantern_turns))
            return false;
    } else {
        h.lantern_turns = 0;
    }

    if (version >= 5) {
        for (int& count : h.weapon_counts)
            if (!read_int(in, count))
                return false;
        for (int& count : h.armor_counts)
            if (!read_int(in, count))
                return false;
    } else {
        h.weapon_counts.fill(0);
        h.armor_counts.fill(0);
        for (std::size_t i = 0; i < bx::weapons.size(); ++i)
            if (bx::weapons[i].id == h.weapon_id) h.weapon_counts[i] = 1;
        for (std::size_t i = 0; i < bx::armor_items.size(); ++i)
            if (bx::armor_items[i].id == h.armor_id) h.armor_counts[i] = 1;
        if (h.has_shield) h.armor_counts[*bx::armor_index("shield")] = 1;
    }

    // Restore runtime-only fields
    h.type  = EntityType::Hero;
    h.alive = true;
    h.hidden = false;
    h.charmed = false;
    h.gold_pieces = 0;
    h.morale = 0;
    h.fleeing = false;
    h.attacks_per_turn = 1;
    h.glyph = '@';
    h.xp_value = 0;
    h.undead = false;
    h.has_poison = false;
    h.has_disease = false;

    // Trailing bytes after a complete, valid load are tolerated; the current
    // writer emits nothing after armor_counts, so future fields appended at
    // the tail of the format can be ignored by older loaders.
    return true;
}

// Deterministic filename sanitization for hero-derived save names. Only a
// conservative ASCII safe set survives; everything else becomes an underscore,
// with consecutive unsafe characters collapsing to one. The result is a single
// path component with no separators, no "..", and no control characters, so a
// hero name can never escape the saves directory.
std::string sanitize_hero_filename(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    bool sep = false;
    for (unsigned char c : name) {
        const bool safe = (c >= 'a' && c <= 'z') ||
                          (c >= 'A' && c <= 'Z') ||
                          (c >= '0' && c <= '9') ||
                          c == '_' || c == '-' || c == ' ';
        if (safe) {
            out.push_back(static_cast<char>(c));
            sep = false;
        } else if (!sep) {
            out.push_back('_');
            sep = true;
        }
    }
    while (!out.empty() && (out.back() == '_' || out.back() == ' '))
        out.pop_back();
    if (out.empty()) return "hero";
    return out;
}

// Append every *.dat file under `dir` to `out`. Uses std::error_code overloads
// so a missing directory or a permission error is handled gracefully instead
// of throwing.
void append_dat_files(const std::filesystem::path& dir,
                      std::vector<std::filesystem::path>& out) {
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec) return;
    std::filesystem::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        const auto& p = it->path();
        if (p.extension() == ".dat")
            out.push_back(p);
    }
}

} // namespace

std::filesystem::path saves_directory() {
    const char* home = std::getenv("HOME");
    if (home && *home)
        return std::filesystem::path(home) / ".bx_crawl" / "saves";
    return std::filesystem::path("bx_crawl_saves");
}

std::filesystem::path save_path_for_hero(const std::string& name) {
    return saves_directory() / (sanitize_hero_filename(name) + ".dat");
}

std::vector<std::filesystem::path> enumerate_saves() {
    std::vector<std::filesystem::path> result;
    append_dat_files(saves_directory(), result);
    return result;
}

std::string strip_save_extension(std::filesystem::path p) {
    return p.stem().string();
}

std::filesystem::path pregenerated_saves_directory(const std::filesystem::path& data_dir) {
    return data_dir / "pregenerated_saves";
}

std::vector<std::filesystem::path> enumerate_all_saves(const std::filesystem::path& data_dir) {
    std::vector<std::filesystem::path> result;
    append_dat_files(saves_directory(), result);
    append_dat_files(pregenerated_saves_directory(data_dir), result);
    return result;
}

std::filesystem::path find_save(const std::string& name, const std::filesystem::path& data_dir) {
    const std::string safe = sanitize_hero_filename(name);

    auto user_path = save_path_for_hero(name);
    {
        std::error_code ec;
        if (std::filesystem::exists(user_path, ec) && !ec)
            return user_path;
    }

    auto pregen_path = pregenerated_saves_directory(data_dir) / (safe + ".dat");
    {
        std::error_code ec;
        if (std::filesystem::exists(pregen_path, ec) && !ec)
            return pregen_path;
    }

    return {};
}

bool save_game(const Game& g, const std::filesystem::path& path) {
    // Ensure parent directory exists
    auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) return false;
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    // Magic + version
    if (!write_exact(out, "BXSV", 4)) return false;
    int32_t version = 5;
    if (!write_int(out, version)) return false;

    const auto& h = g.hero;

    // Game-level state
    if (!write_int(out, static_cast<int32_t>(g.state))) return false;
    if (!write_int(out, g.total_gold)) return false;
    if (!write_int(out, g.total_xp)) return false;
    if (!write_int(out, g.hero_level)) return false;
    if (!write_int(out, g.dungeon_level)) return false;

    // Hero data
    if (!write_str(out, h.name)) return false;
    if (!write_int(out, h.hp)) return false;
    if (!write_int(out, h.max_hp)) return false;
    if (!write_int(out, h.attack_bonus)) return false;
    if (!write_int(out, h.damage_die)) return false;
    if (!write_int(out, h.damage_bonus)) return false;
    if (!write_int(out, h.ac)) return false;

    if (!write_int(out, h.abilities.str)) return false;
    if (!write_int(out, h.abilities.intelligence)) return false;
    if (!write_int(out, h.abilities.wis)) return false;
    if (!write_int(out, h.abilities.dex)) return false;
    if (!write_int(out, h.abilities.con)) return false;
    if (!write_int(out, h.abilities.cha)) return false;

    if (!write_int(out, static_cast<int32_t>(h.char_class))) return false;
    if (!write_int(out, h.torch_turns)) return false;
    if (!write_int(out, h.light_spell_turns)) return false;
    if (!write_int(out, h.knives)) return false;
    if (!write_int(out, h.darts)) return false;
    if (!write_int(out, h.shield_turns)) return false;
    if (!write_int(out, h.protection_turns)) return false;
    if (!write_int(out, h.courage_turns)) return false;
    if (!write_int(out, h.resist_cold_turns)) return false;

    if (!write_str(out, h.weapon_id)) return false;
    if (!write_str(out, h.armor_id)) return false;
    if (!write_int(out, h.has_shield ? 1 : 0)) return false;

    if (!write_int(out, h.skill_hide)) return false;
    if (!write_int(out, h.skill_pick_locks)) return false;
    if (!write_int(out, h.skill_find_traps)) return false;
    if (!write_int(out, h.skill_climb)) return false;
    if (!write_int(out, h.skill_move_silent)) return false;
    if (!write_int(out, h.skill_hear_noise)) return false;

    // Known spells
    if (!write_int(out, static_cast<int32_t>(h.spells.size()))) return false;
    for (const auto& s : h.spells)
        if (!write_str(out, s)) return false;

    // Spell slots (including index 0 - unused placeholder)
    for (int i = 0; i <= Entity::MaxSpellLevel; ++i) {
        if (!write_int(out, h.spell_slots[i])) return false;
        if (!write_int(out, h.max_spell_slots[i])) return false;
    }

    // Version 2+ fields
    if (!write_int(out, h.rations)) return false;
    if (!write_int(out, h.turns_without_food)) return false;
    if (!write_int(out, h.oil_flasks)) return false;
    if (!write_int(out, h.holy_water)) return false;
    if (!write_int(out, h.ai_patrol_x)) return false;
    if (!write_int(out, h.ai_patrol_y)) return false;
    if (!write_int(out, h.ai_wander_timer)) return false;

    // Version 3+ fields
    for (int count : h.gear_counts)
        if (!write_int(out, count)) return false;

    // Version 4+ fields
    if (!write_int(out, h.lantern_turns)) return false;

    // Version 5+ fields
    for (int count : h.weapon_counts)
        if (!write_int(out, count)) return false;
    for (int count : h.armor_counts)
        if (!write_int(out, count)) return false;

    out.flush();
    if (!out) return false;
    out.close();
    return out.good();
}

bool load_game(Game& g, const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    // Deserialize into a temporary copy of the current runtime state. Only on
    // a fully successful, validated read is the live state replaced, so a
    // corrupt or truncated save can never partially mutate `g`.
    Game tmp = g;
    if (!read_game_from(in, tmp)) return false;
    g = std::move(tmp);
    return true;
}
