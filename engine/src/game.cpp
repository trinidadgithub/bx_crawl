#include "game.hpp"
#include "game_internal.hpp"
#include "game_snapshot.hpp"
#include "map.hpp"
#include "bx_data.hpp"
#include "item_effects.hpp"
#include "inventory.hpp"

#include <algorithm>
#include <iostream>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

// ---------------------------
// Random helper
// ---------------------------
int roll_d(Game& g, int sides) {
    return g.rng.d(sides);
}

// Check if monster spawning is disabled via environment
static bool monsters_disabled() {
    const char* env = std::getenv("BX_NO_MONSTERS");
    return env && env[0] == '1';
}

// ---------------------------
// Internal helpers
// ---------------------------

// Spawned from bx_data by table position. Defined at global scope later in
// this TU (after the anonymous namespace) so the module session can reuse it.
Entity spawn_bx_monster_by_index(Game& g, int idx);

namespace {

void log_event(Game& g, const std::string& msg) {
    g.event_log.push_back(msg);
    g.events.push_back(GameEvent{GameEventType::LogMessage, msg});
    // keep the log small (e.g., last 8 messages)
    const std::size_t max_events = 8;
    if (g.event_log.size() > max_events) {
        g.event_log.erase(g.event_log.begin(),
                          g.event_log.begin() + (g.event_log.size() - max_events));
    }
}

void queue_sound(Game& g, SoundEffect effect) {
    GameEvent event;
    event.type = GameEventType::Sound;
    event.sound = effect;
    g.events.push_back(std::move(event));
}

bool resolve_first_contact_surprise(Game& g, Entity& monster) {
    if (monster.surprise_checked) {
        return false;
    }

    monster.surprise_checked = true;
    if (g.hero.hidden) {
        monster.surprised = true;
        return false;
    }

    SurpriseResult result = roll_surprise(g, monster_surprise_threshold(monster.name));

    if (result.hero_surprised && result.monsters_surprised) {
        log_event(g, "Both sides hesitate in surprise!");
        return false;
    }

    if (result.hero_surprised) {
        log_event(g, "The " + monster.name + " catches you off guard!");
        return true;
    }

    if (result.monsters_surprised) {
        monster.surprised = true;
        log_event(g, "You catch the " + monster.name + " unaware!");
    }

    return false;
}

// B/X ability modifier from bx_data tables, for display/logging.
int ability_modifier_display(int score) {
    return bx::get_strength(score).toHitMelee;
}

// Class hit-die sides
int class_hd_sides(CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return 8;
        case CharacterClass::Cleric:    return 6;
        case CharacterClass::Thief:     return 4;
        case CharacterClass::MagicUser: return 4;
        case CharacterClass::Dwarf:     return 8;
        case CharacterClass::Elf:       return 6;
        case CharacterClass::Halfling:  return 6;
        default:                        return 8;
    }
}

int fighter_attack_bonus_for_level(int level) {
    if (level <= 3) return 0;
    if (level <= 6) return 2;
    if (level <= 9) return 5;
    if (level <= 12) return 7;
    if (level <= 15) return 9;
    return 11;
}

// Monster XP values (B/X Moldvay)
int monster_xp_value(const Entity& m) {
    if (m.type == EntityType::Monster) return m.xp_value;
    switch (m.type) {
        case EntityType::GiantRat:   return 5;
        case EntityType::Kobold:     return 5;
        case EntityType::Goblin:     return 10;
        case EntityType::Skeleton:   return 10;
        case EntityType::Orc:        return 10;
        case EntityType::GiantSpider: return 13;
        case EntityType::Zombie:     return 20;
        default:                     return 5;
    }
}

// XP needed to reach a given level, class-aware via bx_data tables.
// Falls back to a generic formula for CharacterClass::None or beyond the table.
int xp_for_level(CharacterClass cc, int level) {
    if (level <= 1) return 0;

    auto lookup = [&](const auto& table) -> int {
        for (const auto& entry : table)
            if (entry.level == level) return entry.xp_required;
        return -1;
    };

    int xp = -1;
    switch (cc) {
        case CharacterClass::Fighter:   xp = lookup(bx::fighter_levels);   break;
        case CharacterClass::MagicUser: xp = lookup(bx::magic_user_levels); break;
        case CharacterClass::Thief:     xp = lookup(bx::thief_levels);     break;
        case CharacterClass::Cleric:    xp = lookup(bx::cleric_levels);    break;
        case CharacterClass::Dwarf:     xp = lookup(bx::dwarf_levels);     break;
        case CharacterClass::Elf:       xp = lookup(bx::elf_levels);       break;
        case CharacterClass::Halfling:  xp = lookup(bx::halfling_levels);  break;
        default: break;
    }
    if (xp >= 0) return xp;

    // Generic fallback
    if (level >= 9) return 240000 + (level - 9) * 120000;
    return 2000 * (1 << (level - 2));
}

void award_treasure(Game& g, Entity& monster) {
    int gold = monster.gold_pieces;
    if (gold > 0) {
        g.total_gold += gold;
        {
            std::ostringstream oss;
            oss << monster.name << " drops " << gold << " gp.";
            log_event(g, oss.str());
        }
    }

    // Chance to drop a torch
    if (in_bounds(g, monster.pos.x, monster.pos.y) && roll_d(g, 2) == 1) {
        g.map[index_for(g, monster.pos.x, monster.pos.y)].items.push_back(ItemType::Torch);
        log_event(g, monster.name + " drops a torch.");
    }

    int xp_gain = monster_xp_value(monster) + gold;
    g.total_xp += xp_gain;

    {
        std::ostringstream oss;
        oss << "Gained " << xp_gain << " XP (total: " << g.total_xp << ").";
        log_event(g, oss.str());
    }
}

void check_level_up(Game& g) {
    int needed = xp_for_level(g.hero.char_class, g.hero_level + 1);
    while (g.total_xp >= needed && g.hero_level < 14) {
        int con_mod = ability_modifier_display(g.hero.abilities.con);
        int hd = class_hd_sides(g.hero.char_class);
        int hp_gain = roll_d(g, hd) + con_mod;
        hp_gain = std::max(1, hp_gain);

        g.hero_level++;
        g.hero.max_hp += hp_gain;
        g.hero.hp     += hp_gain;
        if (g.hero.char_class == CharacterClass::Fighter) {
            int str_mod = bx::get_strength(g.hero.abilities.str).toHitMelee;
            g.hero.attack_bonus = fighter_attack_bonus_for_level(g.hero_level) + str_mod;
        } else {
            g.hero.attack_bonus++;
        }

        // Grant new spell slots from bx_data class tables
        auto grant_slots = [&](const auto& table) {
            for (const auto& entry : table) {
                if (entry.level == g.hero_level) {
                    g.hero.max_spell_slots[1] = entry.first_level_spells;
                    g.hero.max_spell_slots[2] = entry.second_level_spells;
                    g.hero.spell_slots[1]      = entry.first_level_spells;
                    g.hero.spell_slots[2]      = entry.second_level_spells;
                    break;
                }
            }
        };
        switch (g.hero.char_class) {
            case CharacterClass::MagicUser: grant_slots(bx::magic_user_levels); break;
            case CharacterClass::Cleric:    grant_slots(bx::cleric_levels);    break;
            case CharacterClass::Elf:       grant_slots(bx::elf_levels);       break;
            default: break;
        }

        {
            std::ostringstream oss;
            oss << g.hero.name << " reaches level " << g.hero_level
                << "! HP +" << hp_gain
                << " (1d" << hd << ")"
                << ", attack bonus now " << g.hero.attack_bonus;
            log_event(g, oss.str());
        }

        needed = xp_for_level(g.hero.char_class, g.hero_level + 1);
    }
}

// Check if a tile is occupied by any living entity (hero or monsters).
bool is_occupied(const Game& g, int x, int y, const Entity* ignore = nullptr) {
    // Hero
    if (g.hero.alive &&
        &g.hero != ignore &&
        g.hero.pos.x == x && g.hero.pos.y == y) {
        return true;
    }

    // Monsters
    for (const auto& m : g.monsters) {
        if (!m.alive) continue;
        if (&m == ignore) continue;
        if (m.pos.x == x && m.pos.y == y) {
            return true;
        }
    }

    return false;
}

// Returns true if a tile is carved (passable) — floor, door, or stairs
static bool is_carved(const Game& g, int x, int y) {
    if (!in_bounds(g, x, y)) return false;
    auto t = g.map[index_for(g, x, y)].type;
    return t == TileType::Floor || t == TileType::DoorClosed
        || t == TileType::SecretDoorClosed || t == TileType::DoorOpen
        || t == TileType::StairsDown || t == TileType::StairsUp;
}

static bool is_hidden_secret_door(const Tile& t) {
    return t.type == TileType::SecretDoorClosed && !t.secret_revealed;
}

static bool is_wall_like(const Tile& t) {
    return t.type == TileType::Wall || is_hidden_secret_door(t);
}

// A wall tile orthogonally adjacent to a carved tile
static bool is_border_wall(const Game& g, int x, int y) {
    if (!in_bounds(g, x, y)) return false;
    if (!is_wall_like(g.map[index_for(g, x, y)])) return false;
    return is_carved(g, x, y - 1) || is_carved(g, x, y + 1)
        || is_carved(g, x - 1, y) || is_carved(g, x + 1, y);
}

// A wall tile diagonally adjacent to a carved tile
static bool has_diagonal_carved(const Game& g, int x, int y) {
    if (!in_bounds(g, x, y)) return false;
    if (!is_wall_like(g.map[index_for(g, x, y)])) return false;
    return is_carved(g, x - 1, y - 1) || is_carved(g, x + 1, y - 1)
        || is_carved(g, x - 1, y + 1) || is_carved(g, x + 1, y + 1);
}

// Whether a wall tile should appear on screen:
//   - border wall (orth carved neighbor), or
//   - diagonal-only tile with at least one border-wall neighbor (corners)
// This limits display to a single layer around carved areas.
static bool is_visible_wall(const Game& g, int x, int y) {
    if (!in_bounds(g, x, y)) return false;
    if (!is_wall_like(g.map[index_for(g, x, y)])) return false;
    if (is_border_wall(g, x, y)) return true;
    if (!has_diagonal_carved(g, x, y)) return false;
    return is_border_wall(g, x - 1, y) || is_border_wall(g, x + 1, y)
        || is_border_wall(g, x, y - 1) || is_border_wall(g, x, y + 1);
}

// Box-drawing glyph connecting adjacent visible-wall tiles
static const char* wall_glyph(const Game& game, int x, int y) {
    bool wr = is_visible_wall(game, x + 1, y);
    bool wl = is_visible_wall(game, x - 1, y);
    bool wu = is_visible_wall(game, x, y - 1);
    bool wd = is_visible_wall(game, x, y + 1);

    if (wr && wl && wu && wd) return "\xe2\x94\xbc"; // ┼
    if (wr && wl && wd)       return "\xe2\x94\xac"; // ┬
    if (wr && wl && wu)       return "\xe2\x94\xb4"; // ┴
    if (wr && wu && wd)       return "\xe2\x94\x9c"; // ├
    if (wl && wu && wd)       return "\xe2\x94\xa4"; // ┤
    if (wr && wd)             return "\xe2\x94\x8c"; // ┌
    if (wl && wd)             return "\xe2\x94\x90"; // ┐
    if (wr && wu)             return "\xe2\x94\x94"; // └
    if (wl && wu)             return "\xe2\x94\x98"; // ┘
    if (wl && wr)             return "\xe2\x94\x80"; // ─
    if (wu && wd)             return "\xe2\x94\x82"; // │
    if (wl || wr)             return "\xe2\x94\x80"; // ─
    if (wu || wd)             return "\xe2\x94\x82"; // │
    return " ";
}

char glyph_for(const Entity& e) {
    if (e.type == EntityType::Monster) return e.glyph;
    switch (e.type) {
        case EntityType::Hero:       return '@';
        case EntityType::Goblin:     return 'g';
        case EntityType::Kobold:     return 'k';
        case EntityType::GiantRat:   return 'r';
        case EntityType::Skeleton:   return 's';
        case EntityType::Orc:        return 'o';
        case EntityType::GiantSpider: return 'S';
        case EntityType::Zombie:     return 'z';
        default:                     return '?';
    }
}

int sign(int v) {
    if (v < 0) return -1;
    if (v > 0) return 1;
    return 0;
}

void move_entity(Game& g, Entity& e, int dx, int dy) {
    if (!e.alive) return;
    if (dx == 0 && dy == 0) return;

    int nx = e.pos.x + dx;
    int ny = e.pos.y + dy;

    if (!is_blocked(g, nx, ny) && !is_occupied(g, nx, ny, &e)) {
        e.pos.x = nx;
        e.pos.y = ny;
    }
}

// Find the first living monster at (x, y), or nullptr if none.
Entity* monster_at(Game& g, int x, int y) {
    for (auto& m : g.monsters) {
        if (!m.alive) continue;
        if (m.pos.x == x && m.pos.y == y) {
            return &m;
        }
    }
    return nullptr;
}

// 2d6 morale roll
int roll_2d6(Game& g) {
    return roll_d(g, 6) + roll_d(g, 6);
}

// Shared monster template data
struct MonsterTemplate {
    EntityType type;
    const char* name;
    int hp;
    int atk_bonus;
    int damage_die;
    int ac;
    int morale;
    int xp;
};

inline const MonsterTemplate monster_templates[] = {
    {EntityType::Goblin,     "Goblin",     5, 1, 6, 12, 8,  10},
    {EntityType::Kobold,     "Kobold",     3, 0, 4, 11, 6,   5},
    {EntityType::GiantRat,   "Giant Rat",  2, 0, 3, 12, 8,   5},
    {EntityType::Skeleton,   "Skeleton",   4, 0, 6, 12, 12, 10},
    {EntityType::Orc,        "Orc",        5, 1, 6, 13, 8,  10},
    {EntityType::GiantSpider,"Giant Spider",4, 0, 6, 11, 8,  13},
    {EntityType::Zombie,     "Zombie",     9, 1, 6, 11, 12,  20},
};
const int num_monster_types = 7;

// ── Data-driven monster helpers (bx_data integration) ──────

// Parse a hit-dice string and return average HP and effective HD count.
// Accepts formats: "1", "2", "1+1", "2+2", "1/2 (1-4 hp)", "1 point", etc.
struct ParsedHD {
    int hd_count;    // number of hit dice (0 = fractional)
    int hp_per_hd;   // typically 4.5 (1d8) in B/X
    int bonus_hp;    // flat bonus (e.g. "2+2" → bonus=2)
    int avg_hp;      // pre-computed average HP
};
static ParsedHD parse_hit_dice(const char* hd) {
    ParsedHD result{1, 5, 0, 5}; // default: 1 HD, ~5 HP
    if (!hd || !*hd) return result;

    std::string s(hd);

    // "1 point" or "1/2 (1-4 hp)" or similar fractional
    if (s.find("point") != std::string::npos) {
        result.hd_count = 0;
        result.avg_hp = 1;
        return result;
    }
    if (s.find("1/2") != std::string::npos) {
        result.hd_count = 0;
        result.avg_hp = 3; // average of 1-4
        return result;
    }

    // Strip asterisk
    auto star = s.find('*');
    if (star != std::string::npos) s.erase(star);

    // Strip trailing non-numeric after the HD expression
    auto paren = s.find('(');
    if (paren != std::string::npos) s.erase(paren);

    // Parse "N+M" or "N-M" or "N"
    int hd_num = 1, bonus = 0;
    auto plus = s.find('+');
    if (plus == std::string::npos) plus = s.find('-');
    if (plus != std::string::npos) {
        hd_num = std::atoi(s.c_str());
        bonus = std::atoi(s.c_str() + plus + 1);
        if (s[plus] == '-') bonus = -bonus;
    } else {
        hd_num = std::atoi(s.c_str());
        if (hd_num == 0) hd_num = 1;
    }

    result.hd_count = hd_num;
    result.bonus_hp = bonus;
    // B/X HD are d8, average 4.5 per die
    int raw = hd_num * 5 + bonus; // approximate using 5 per die
    result.avg_hp = raw < 1 ? 1 : raw;
    return result;
}

// Parse a damage string like "1-6", "2-8", "1-4/1-4" and return the primary die size.
static int parse_damage_die(const char* dmg) {
    if (!dmg || !*dmg) return 6;
    std::string s(dmg);

    // "by spell", "by weapon", "special", "paralysis", "nil" → default
    if (s.find("by ") == 0 || s == "special" || s == "paralysis"
        || s == "nil" || s == "Nil") return 6;

    // Take first damage expression before '/'
    auto slash = s.find('/');
    if (slash != std::string::npos) s = s.substr(0, slash);

    // "or weapon" → skip, use default
    if (s.find("or weapon") != std::string::npos) return 6;
    // "or " → take first part
    auto or_pos = s.find(" or ");
    if (or_pos != std::string::npos) s = s.substr(0, or_pos);

    // "1-6" → die=6
    auto dash = s.find('-');
    if (dash != std::string::npos) {
        return std::atoi(s.c_str() + dash + 1);
    }
    // "Nil" → default
    return 6;
}

// Choose a monster glyph based on name (Roguelike convention: first letter lowercase)
static char monster_glyph(const char* name) {
    if (!name || !*name) return '?';
    char c = name[0];
    if (c >= 'A' && c <= 'Z') c += 32; // lowercase

    // Special cases for common letters
    if (c == 'a') return 'A'; // Ape, Acolyte — use uppercase A
    // Default: use first letter
    return c;
}

// Pick a random monster from bx_data suitable for the given dungeon level.
// Uses HD as a proxy for difficulty: level N gets monsters with HD roughly N or N/2+1.
static int choose_monster_index(Game& g, int dungeon_level) {
    // Target HD range: allow monsters with HD in [lo, hi]
    int lo = std::max(0, (dungeon_level - 1) / 2);
    int hi = dungeon_level + 1;

    // Collect candidate indices
    std::vector<int> candidates;
    for (std::size_t i = 0; i < bx::monsters.size(); ++i) {
        const auto& m = bx::monsters[i];
        // Skip NPC parties and duplicate entries
        if (std::string_view(m.name).find("NPC Party") != std::string_view::npos) continue;
        if (m.morale == 0) continue; // invalid entries

        // Parse HD
        int hd = 1;
        std::string s(m.hit_dice);
        auto star = s.find('*');
        if (star != std::string::npos) s.erase(star);
        auto paren = s.find('(');
        if (paren != std::string::npos) s.erase(paren);
        auto plus = s.find('+');
        if (plus == std::string::npos) plus = s.find('-');
        if (plus != std::string::npos) {
            hd = std::atoi(s.c_str());
        } else if (s.find("point") != std::string::npos) {
            hd = 0;
        } else if (s.find("1/2") != std::string::npos) {
            hd = 0;
        } else {
            hd = std::atoi(s.c_str());
        }

        if (hd >= lo && hd <= hi) {
            candidates.push_back(static_cast<int>(i));
        }
    }

    if (candidates.empty()) {
        // Fallback: any monster
        return roll_d(g, static_cast<int>(bx::monsters.size())) - 1;
    }

    return candidates[roll_d(g, static_cast<int>(candidates.size())) - 1];
}

static std::string normalized_monster_name(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char ch : s) {
        if (std::isalnum(ch)) {
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return out;
}

// Developer override: BX_TEST_MONSTER=<name-or-1-based-index> spawns exactly one
// requested monster and disables wandering monsters for focused behavior tests.
static int test_monster_index() {
    const char* env = std::getenv("BX_TEST_MONSTER");
    if (!env || !*env) return -1;

    std::string requested(env);
    bool numeric = !requested.empty();
    for (unsigned char ch : requested) {
        if (!std::isdigit(ch)) {
            numeric = false;
            break;
        }
    }
    if (numeric) {
        int one_based = std::atoi(requested.c_str());
        if (one_based >= 1 && one_based <= static_cast<int>(bx::monsters.size())) {
            return one_based - 1;
        }
        return -1;
    }

    std::string needle = normalized_monster_name(requested);
    if (needle.empty()) return -1;

    for (std::size_t i = 0; i < bx::monsters.size(); ++i) {
        if (normalized_monster_name(bx::monsters[i].name) == needle) {
            return static_cast<int>(i);
        }
    }

    for (std::size_t i = 0; i < bx::monsters.size(); ++i) {
        if (normalized_monster_name(bx::monsters[i].name).find(needle) != std::string::npos) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

// Compute XP value for a monster from bx_data.
// B/X formula: HD < 1 → 5 XP; HD 1 → 10 XP; HD 1+ → +75 XP per HD (roughly)
static int compute_xp_value(const char* hit_dice) {
    int hd = 1;
    std::string s(hit_dice);
    auto star = s.find('*');
    if (star != std::string::npos) s.erase(star);
    auto paren = s.find('(');
    if (paren != std::string::npos) s.erase(paren);
    auto plus = s.find('+');
    if (plus == std::string::npos) plus = s.find('-');
    if (plus != std::string::npos) {
        hd = std::atoi(s.c_str());
    } else if (s.find("point") != std::string::npos) {
        hd = 0;
    } else if (s.find("1/2") != std::string::npos) {
        hd = 0;
    } else {
        hd = std::atoi(s.c_str());
    }
    if (hd <= 0) return 5;
    if (hd == 1) return 10;
    return 10 + (hd - 1) * 75; // rough B/X approximation
}

// Check if a monster name indicates undead
static bool is_name_undead(const char* name) {
    std::string_view n(name);
    return n.find("Skeleton") != std::string_view::npos
        || n.find("Zombie") != std::string_view::npos
        || n.find("Ghoul") != std::string_view::npos
        || n.find("Wight") != std::string_view::npos
        || n.find("Wraith") != std::string_view::npos
        || n.find("Shadow") != std::string_view::npos
        || n.find("Mummy") != std::string_view::npos;
}

// Check if a monster name indicates poison attack
static bool is_name_poison(const char* name) {
    std::string_view n(name);
    return n.find("poison") != std::string_view::npos
        || n.find("Spider") != std::string_view::npos
        || n.find("Cobra") != std::string_view::npos
        || n.find("Viper") != std::string_view::npos
        || n.find("Centipede") != std::string_view::npos
        || n.find("Scorpion") != std::string_view::npos;
}

// Check if a monster name indicates disease attack
static bool is_name_disease(const char* name) {
    std::string_view n(name);
    return n.find("disease") != std::string_view::npos
        || n.find("Rat,") != std::string_view::npos
        || n.find("Tick") != std::string_view::npos;
}

// Check if a monster name indicates a paralysis attack (save vs. Paralysis)
static bool is_name_paralysis(const char* name) {
    std::string_view n(name);
    return n.find("Ghoul") != std::string_view::npos
        || n.find("Carrion Crawler") != std::string_view::npos
        || n.find("Gelatinous Cube") != std::string_view::npos
        || n.find("Thoul") != std::string_view::npos;
}

// Check if a monster name is a normal bat (confusion, no attack)
static bool is_name_normal_bat(const char* name) {
    return std::string_view(name) == "Bat, Normal";
}

// Check if a monster name is a giant bat (vampire bat variant)
static bool is_name_giant_bat(const char* name) {
    return std::string_view(name) == "Bat, Giant";
}

// B/X: 10+ normal bats flying around the hero's head confuse them.
// Confused characters: -2 to hit, -2 on saves, cannot cast spells.
static bool hero_is_confused(const Game& g) {
    if (g.hero.paralyzed_turns > 0) return false;
    if (g.hero.unconscious_turns > 0) return false;
    int total = 0;
    for (const auto& m : g.monsters) {
        if (!m.alive) continue;
        if (m.charmed) continue;
        if (!is_name_normal_bat(m.name.c_str())) continue;
        // Bats must be close enough to swarm around the hero's head.
        int dist = std::abs(g.hero.pos.x - m.pos.x) + std::abs(g.hero.pos.y - m.pos.y);
        if (dist > 8) continue;
        total += m.count;
    }
    return total >= 10;
}

// Hero's light radius from the active light source. Demihuman infravision
// gives a small radius with no actual light.
static int hero_light_radius(const Game& g) {
    if (g.hero.light_spell_turns > 0) return 10;
    if (g.hero.lantern_turns > 0) return 6;
    if (g.hero.torch_turns > 0) return 6;
    switch (g.hero.char_class) {
        case CharacterClass::Dwarf:
        case CharacterClass::Elf:
        case CharacterClass::Halfling:
            return 2;
        default:
            return 0;
    }
}

// Does the hero have an actual light source (not infravision)?
static bool hero_carries_light(const Game& g) {
    return g.hero.light_spell_turns > 0
        || g.hero.lantern_turns > 0
        || g.hero.torch_turns > 0;
}

// B/X lair humanoids that suffer a -1 to-hit penalty in full daylight.
static bool is_name_daylight_sensitive(const char* name) {
    std::string_view n(name);
    return n == "Goblin" || n == "Orc";
}

// B/X: goblins and orcs were used to the dark. Fighting in the hero's light
// (i.e. carrying light into their lair) gives them a -1 to-hit penalty.
// The goblin king and his bodyguard are used to light and fight unhindered.
static int daylight_to_hit_penalty(const Game& g, const Entity& m) {
    if (!is_name_daylight_sensitive(m.name.c_str())) return 0;
    if (m.is_leader) return 0;
    if (!hero_carries_light(g)) return 0;
    int r = hero_light_radius(g);
    int dist = std::abs(g.hero.pos.x - m.pos.x) + std::abs(g.hero.pos.y - m.pos.y);
    if (dist > r) return 0;
    return 1;
}

// B/X lair humanoids with a lair leader wielding morale as a collective.
static bool is_name_lair_humanoid(const char* name) {
    std::string_view n(name);
    return n == "Goblin" || n == "Hobgoblin" || n == "Kobold" || n == "Orc";
}

// Is a living leader of the monster's kind close enough to inspire it?
static bool leader_inspiring(const Game& g, const Entity& m) {
    if (!m.alive) return false;
    if (!is_name_lair_humanoid(m.name.c_str())) return false;
    for (const auto& other : g.monsters) {
        if (&other == &m) continue;
        if (!other.alive || !other.is_leader) continue;
        if (other.name != m.name) continue;
        int dist = std::abs(other.pos.x - m.pos.x) + std::abs(other.pos.y - m.pos.y);
        if (dist <= 8) return true;
    }
    return false;
}

// B/X morale with lair leaders: while their king/chieftain lives nearby the
// group fights bravely; once the leader falls they fall back on their base
// morale. Non-lair creatures always use their own morale.
static int effective_morale(const Game& g, const Entity& m) {
    if (!is_name_lair_humanoid(m.name.c_str())) return m.morale;
    if (!leader_inspiring(g, m)) return m.morale;
    if (m.name == "Goblin")    return 9;
    if (m.name == "Hobgoblin") return 10;
    if (m.name == "Kobold")    return 8;
    if (m.name == "Orc")       return 8;
    return m.morale;
}

static Entity spawn_bx_monster(Game& g, int dungeon_level) {
    return spawn_bx_monster_by_index(g, choose_monster_index(g, dungeon_level));
}

static Entity spawn_test_monster(Game& g) {
    return spawn_bx_monster_by_index(g, test_monster_index());
}

// Promote one creature of each lair humanoid type to its B/X lair leader.
// Killing the leader removes the morale so the group fights at a lower value.
static void promote_lair_leaders(Game& g) {
    struct LeaderSpec { const char* name; int hp; int atk; int dmg; };
    const LeaderSpec spec[] = {
        {"Goblin",    15, 3, 1},
        {"Orc",        8, 1, 1},
        {"Kobold",     9, 2, 0},
        {"Hobgoblin", 22, 5, 2},
    };
    for (const LeaderSpec& s : spec) {
        for (auto& m : g.monsters) {
            if (m.alive && !m.is_leader && m.name == s.name) {
                m.is_leader = true;
                m.max_hp = s.hp;
                m.hp = s.hp;
                m.attack_bonus = s.atk;
                m.damage_bonus += s.dmg;
                break;
            }
        }
    }
}

// Saving throw helper
static bool make_save(Game& g, CharacterClass cc, int category) {
    bx::SaveClass sc = [&]() {
        switch (cc) {
            case CharacterClass::Cleric:    return bx::SaveClass::cleric;
            case CharacterClass::Fighter:   return bx::SaveClass::fighter;
            case CharacterClass::Dwarf:     return bx::SaveClass::dwarf;
            case CharacterClass::Elf:       return bx::SaveClass::elf;
            case CharacterClass::Halfling:  return bx::SaveClass::halfling;
            case CharacterClass::MagicUser: return bx::SaveClass::magicUser;
            case CharacterClass::Thief:     return bx::SaveClass::thief;
            default:                        return bx::SaveClass::fighter;
        }
    }();
    const bx::SaveRow& row = bx::saving_throws[static_cast<int>(sc)];
    int target = 0;
    switch (category) {
        case 0: target = row.death_ray_poison; break;
        case 1: target = row.magic_wands; break;
        case 2: target = row.paralysis_petrify; break;
        case 3: target = row.dragon_breath; break;
        case 4: target = row.rods_spells; break;
        default: target = row.death_ray_poison; break;
    }
    int roll = roll_d(g, 20);
    // Confused by bats: -2 on saves
    if (hero_is_confused(g)) roll -= 2;
    int wis_mod = bx::get_wisdom(g.hero.abilities.wis).savingThrowMagic;
    if (category == 4) roll += wis_mod;
    return roll >= target;
}

void seed_rng(Game& g) {
    auto seed = static_cast<unsigned long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
    );
    g.rng.seed(seed);
}

void init_runtime_state(Game& g, const std::filesystem::path& data_dir, bool should_seed_rng) {
    g.width  = 40;
    g.height = 20;

    if (should_seed_rng) {
        seed_rng(g);
    }

    const auto descriptions_path = data_dir / "descriptions.txt";
    if (!g.descriptions.load_from_file(descriptions_path.string())) {
        std::cerr << "Warning: failed to load descriptions.txt\n";
    }

    g.data_dir = data_dir;
    g.map.clear();
    g.rooms.clear();
    g.monsters.clear();
    g.event_log.clear();
    g.wander_counter = 0;
    g.tick_clock = 0;
    g.town_submenu = 0;
}

void init_game_with_hero(Game& g, Entity hero, const std::filesystem::path& data_dir,
                         bool should_seed_rng) {
    init_runtime_state(g, data_dir, should_seed_rng);

    auto rooms = init_map(g);
    g.rooms = rooms;
    g.dungeon_level = 1;

    g.hero = std::move(hero);
    g.state = GameState::Town;
    g.total_gold = g.hero.gold_pieces;
    g.hero.gold_pieces = 0;

    // Place hero at centre of first room
    if (!rooms.empty()) {
        g.hero.pos = {
            rooms[0].x + rooms[0].w / 2,
            rooms[0].y + rooms[0].h / 2
        };
    } else {
        g.hero.pos = {2, 2};
    }

    // Mark the entire entrance room as explored
    if (!rooms.empty()) {
        const auto& r = rooms[0];
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x)
                g.map[index_for(g, x, y)].explored = true;
    }

    // Helper: pick a random floor tile inside a room
    auto random_room_pos = [&](const Room& r) -> Vec2 {
        int px = r.x + roll_d(g, r.w) - 1;
        int py = r.y + roll_d(g, r.h) - 1;
        return {px, py};
    };

    // Pick a random room index excluding the first one
    auto random_monster_room = [&]() -> int {
        if (rooms.size() <= 1) return 0;
        return roll_d(g, static_cast<int>(rooms.size()) - 1); // 1 .. n-1
    };

    // Place a monster in a random room, retrying if the target tile is occupied
    auto place_monster = [&](Entity& e, const Room& room) {
        for (int attempt = 0; attempt < 20; ++attempt) {
            Vec2 pos = random_room_pos(room);
            if (!is_occupied(g, pos.x, pos.y)) {
                e.pos = pos;
                return;
            }
        }
        // Fallback: place at room centre
        e.pos = {room.x + room.w / 2, room.y + room.h / 2};
    };

    // Spawn initial monsters (unless disabled). BX_TEST_MONSTER spawns exactly one.
    int test_idx = test_monster_index();
    if (test_idx >= 0) {
        Entity e = spawn_test_monster(g);
        place_monster(e, rooms[static_cast<std::size_t>(random_monster_room())]);
        g.monsters.push_back(e);
    } else if (!monsters_disabled()) {
        int num_monsters = 4 + roll_d(g, 3); // 4..6
        for (int i = 0; i < num_monsters; ++i) {
            Entity e = spawn_bx_monster(g, g.dungeon_level);
            place_monster(e, rooms[static_cast<std::size_t>(random_monster_room())]);
            g.monsters.push_back(e);
        }
        promote_lair_leaders(g);
    }

    // Initial location description
    g.current_location_id = "start";
}

// Helper: regenerate dungeon for a new level, preserving hero state
void regenerate_dungeon(Game& g, int new_dungeon_level) {
    g.dungeon_level = new_dungeon_level;
    g.event_log.clear();

    // Regenerate map
    auto rooms = init_map(g);
    g.rooms = rooms;

    // Place hero at entrance
    if (!rooms.empty()) {
        g.hero.pos = {
            rooms[0].x + rooms[0].w / 2,
            rooms[0].y + rooms[0].h / 2
        };
        // Mark entrance room explored
        const auto& r = rooms[0];
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x)
                g.map[index_for(g, x, y)].explored = true;
    } else {
        g.hero.pos = {2, 2};
    }

    // Clear old monsters and spawn new ones, scaled by dungeon level
    g.monsters.clear();

    auto random_room_pos = [&](const Room& r) -> Vec2 {
        int px = r.x + roll_d(g, r.w) - 1;
        int py = r.y + roll_d(g, r.h) - 1;
        return {px, py};
    };

    auto random_monster_room = [&]() -> int {
        if (rooms.size() <= 1) return 0;
        return roll_d(g, static_cast<int>(rooms.size()) - 1);
    };

    auto place_monster = [&](Entity& e, const Room& room) {
        for (int attempt = 0; attempt < 20; ++attempt) {
            Vec2 pos = random_room_pos(room);
            if (!is_occupied(g, pos.x, pos.y)) {
                e.pos = pos;
                return;
            }
        }
        e.pos = {room.x + room.w / 2, room.y + room.h / 2};
    };

    // Spawn monsters for this level (unless disabled). BX_TEST_MONSTER spawns exactly one.
    int test_idx = test_monster_index();
    if (test_idx >= 0) {
        Entity e = spawn_test_monster(g);
        place_monster(e, rooms[static_cast<std::size_t>(random_monster_room())]);
        g.monsters.push_back(e);
    } else if (!monsters_disabled()) {
        int base_count = 4 + roll_d(g, 3);
        int num_monsters = base_count + g.dungeon_level - 1; // +1 per level
        if (num_monsters > 12) num_monsters = 12;

        for (int i = 0; i < num_monsters; ++i) {
            Entity e = spawn_bx_monster(g, g.dungeon_level);
            place_monster(e, rooms[static_cast<std::size_t>(random_monster_room())]);
            g.monsters.push_back(e);
        }
        promote_lair_leaders(g);
    }
}

void descend_stairs(Game& g) {
    int next = g.dungeon_level + 1;
    regenerate_dungeon(g, next);
    log_event(g, "You descend to dungeon level " + std::to_string(next) + ".");
}

void ascend_stairs(Game& g) {
    if (g.dungeon_level <= 1) {
        leave_dungeon(g);
        log_event(g, "You climb out of the dungeon and return to camp.");
        return;
    }
    int prev = g.dungeon_level - 1;
    regenerate_dungeon(g, prev);
    log_event(g, "You climb up to dungeon level " + std::to_string(prev) + ".");
}

} // end anonymous namespace

// Spawn a monster from bx_data. Returns the populated Entity. The helper
// functions it calls live in the anonymous namespace above.
Entity spawn_bx_monster_by_index(Game& g, int idx) {
    if (idx < 0 || idx >= static_cast<int>(bx::monsters.size())) {
        idx = choose_monster_index(g, 1);
    }
    const auto& m = bx::monsters[idx];

    Entity e;
    e.name       = m.name;
    e.type       = EntityType::Monster;
    e.glyph      = monster_glyph(m.name);
    e.ac         = m.ac;

    auto hd = parse_hit_dice(m.hit_dice);
    e.max_hp     = hd.avg_hp;
    e.hp         = hd.avg_hp;
    e.attack_bonus = hd.hd_count; // rough: atk bonus ≈ HD
    e.damage_die = parse_damage_die(m.damage);
    e.morale     = m.morale;
    e.xp_value   = compute_xp_value(m.hit_dice);
    e.undead     = is_name_undead(m.name);
    e.has_poison = is_name_poison(m.name);
    e.has_disease = is_name_disease(m.name);
    e.has_paralysis = is_name_paralysis(m.name);
    e.fleeing    = false;
    e.alive      = true;
    e.attacks_per_turn = 1;

    // Normal bats appear as swarms: a single entity represents a flock.
    // B/X No. Appearing is 1-100; use 2-16 so confusion (10+) is possible.
    if (is_name_normal_bat(m.name)) {
        e.count = roll_d(g, 8) + roll_d(g, 8);
    }

    // Giant bats: 5% of all giant bat encounters are giant vampire bats.
    // A vampire bat's bite causes unconsciousness and lets it drain blood.
    if (is_name_giant_bat(m.name) && roll_d(g, 100) <= 5) {
        e.is_vampire_bat = true;
    }

    // Gold: some monsters carry treasure
    int gold_chance = roll_d(g, 6);
    if (gold_chance <= 2) {
        // 1-2 on 1d6: carries gold based on HD
        e.gold_pieces = roll_d(g, 6) * 10 * std::max(1, hd.hd_count);
    } else {
        e.gold_pieces = 0;
    }

    return e;
}

// Defined later in this TU (wandering monster spawner).
static int spawn_wandering_monsters(Game& g);

namespace game_internal {

void emit_log_event(Game& g, const std::string& msg) {
    log_event(g, msg);
}

void emit_sound(Game& g, SoundEffect effect) {
    queue_sound(g, effect);
}

bool hero_is_confused(const Game& g) {
    return ::hero_is_confused(g);
}

int daylight_to_hit_penalty(const Game& g, const Entity& m) {
    return ::daylight_to_hit_penalty(g, m);
}

int effective_morale(const Game& g, const Entity& m) {
    return ::effective_morale(g, m);
}

void award_treasure(Game& g, Entity& monster) {
    ::award_treasure(g, monster);
}

void check_level_up(Game& g) {
    ::check_level_up(g);
}

bool make_save(Game& g, CharacterClass cc, int category) {
    return ::make_save(g, cc, category);
}

int sign(int v) {
    return ::sign(v);
}

bool is_occupied(const Game& g, int x, int y, const Entity* ignore) {
    return ::is_occupied(g, x, y, ignore);
}

void move_entity(Game& g, Entity& e, int dx, int dy) {
    ::move_entity(g, e, dx, dy);
}

Entity* monster_at(Game& g, int x, int y) {
    return ::monster_at(g, x, y);
}

int spawn_wandering_monsters(Game& g) {
    return ::spawn_wandering_monsters(g);
}

Entity spawn_bx_monster_by_index(Game& g, int idx) {
    return ::spawn_bx_monster_by_index(g, idx);
}

bool resolve_first_contact_surprise(Game& g, Entity& m) {
    return ::resolve_first_contact_surprise(g, m);
}

void descend_stairs(Game& g) {
    ::descend_stairs(g);
}

void ascend_stairs(Game& g) {
    ::ascend_stairs(g);
}

} // namespace game_internal

// ── Equipment helpers ─────────────────────────────────────

static std::string format_coin_purse(int gp) {
    std::ostringstream oss;
    oss << gp << " gp";

    // B/X exchange rates: 1 pp = 5 gp. Prices here are whole gp, so pp+gp
    // is the useful at-a-glance breakdown for shopping.
    int pp = gp / 5;
    int rem_gp = gp % 5;
    if (pp > 0) {
        oss << " (" << pp << " pp";
        if (rem_gp > 0) oss << ", " << rem_gp << " gp";
        oss << ")";
    }
    return oss.str();
}

static std::string format_price_with_purse(int cost_gp, int purse_gp) {
    std::ostringstream oss;
    oss << cost_gp << " gp";
    if (purse_gp >= cost_gp) {
        oss << " [afford]";
    } else {
        oss << " [need " << (cost_gp - purse_gp) << " gp]";
    }
    return oss.str();
}

// Class name helper
const char* class_name(CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return "Fighter";
        case CharacterClass::MagicUser: return "Magic-User";
        case CharacterClass::Thief:     return "Thief";
        case CharacterClass::Cleric:    return "Cleric";
        case CharacterClass::Dwarf:     return "Dwarf";
        case CharacterClass::Elf:       return "Elf";
        case CharacterClass::Halfling:  return "Halfling";
        default:                        return "None";
    }
}

// ---------------------------
// Game initialization
// ---------------------------
void init_game(Game& g, Entity hero) {
    init_game(g, std::move(hero), std::filesystem::path{"../data"});
}

void init_game(Game& g, Entity hero, const std::filesystem::path& data_dir) {
    init_game_with_hero(g, std::move(hero), data_dir, /*should_seed_rng=*/true);
}

void init_game_runtime(Game& g, const std::filesystem::path& data_dir) {
    init_runtime_state(g, data_dir, /*should_seed_rng=*/true);
}

void start_game_with_hero(Game& g, Entity hero, const std::filesystem::path& data_dir) {
    init_game_with_hero(g, std::move(hero), data_dir, /*should_seed_rng=*/false);
}

// ---------------------------
// Rendering
// ---------------------------
static void put_line(const std::string& s) {
    std::cout << s << "\r\n";
}

// ── Town screen rendering ────────────────────────────────

static void render_town_screen(const Game& g) {
    std::cout << "\x1B[2J\x1B[H";

    auto status = [&]() {
        std::ostringstream oss;
        int next_xp = xp_for_level(g.hero.char_class, g.hero_level + 1);
        oss << g.hero.name << "(" << class_name(g.hero.char_class) << ")"
            << "  HP:" << g.hero.hp << "/" << g.hero.max_hp
            << "  Purse:" << format_coin_purse(g.total_gold)
            << "  XP:" << g.total_xp << "/" << next_xp;
        return oss.str();
    };

    auto menu_item = [](const char* key, const char* label) {
        std::cout << "  [" << key << "] " << label << "\r\n";
    };

    switch (g.state) {
    case GameState::Town:
        std::cout << "=== Town Square ===\r\n\r\n";
        menu_item("1", "Tavern — rest, hear rumors");
        menu_item("2", "Temple — healing, donate for XP");
        menu_item("3", "The Saucy Satchel — buy supplies");
        menu_item("4", "Armorer — weapons & armor");
        menu_item("5", "Camp — rest, save, enter dungeon");
        std::cout << "\r\n" << status() << "\r\n";
        std::cout << "[x] Quit\r\n";
        break;

    case GameState::Tavern:
        std::cout << "=== The Laughing Goblin Tavern ===\r\n\r\n";
        std::cout << "\"Welcome, friend! What'll it be?\"\r\n\r\n";
        menu_item("1", "Rest (1 gp — recover 1d3 HP)");
        menu_item("2", "Listen to rumors (free)");
        std::cout << "\r\n" << status() << "\r\n";
        menu_item("t", "Return to town");
        menu_item("c", "Go to camp");
        break;

    case GameState::Temple:
        std::cout << "=== Temple of the Lawful Light ===\r\n\r\n";
        std::cout << "\"The stars speak of your coming. How may we serve?\"\r\n\r\n";
        menu_item("1", "Healing (10 gp — restore 1 HP)");
        menu_item("2", "Donate gold for XP (1 gp = 1 XP, max 100)");
        std::cout << "\r\n" << status() << "\r\n";
        menu_item("t", "Return to town");
        menu_item("c", "Go to camp");
        break;

    case GameState::Shop:
        std::cout << "=== The Saucy Satchel ===\r\n\r\n";
        std::cout << "\"Everything a bold delver needs, packed tight and priced fair.\"\r\n\r\n";
        if (g.town_submenu == 0) {
            menu_item("1", "Browse B/X adventuring gear");
            std::cout << "\r\n" << status() << "\r\n";
            menu_item("t", "Return to town");
            menu_item("c", "Go to camp");
        } else if (g.town_submenu == 4 || g.town_submenu == 5) {
            int base = (g.town_submenu == 4) ? 0 : 10;
            std::cout << "--- B/X Adventuring Gear "
                      << (g.town_submenu == 4 ? "(1/2)" : "(2/2)")
                      << " ---\r\n\r\n";
            for (int i = 0; i < 10 && base + i < static_cast<int>(bx::gear_items.size()); ++i) {
                const auto& item = bx::gear_items[static_cast<std::size_t>(base + i)];
                char key = (i == 9) ? '0' : static_cast<char>('1' + i);
                std::cout << "  [" << key << "] " << item.name
                          << " — " << format_price_with_purse(item.cost_gp, g.total_gold)
                          << "  owned:" << g.hero.gear_counts[static_cast<std::size_t>(base + i)]
                          << "\r\n";
            }
            std::cout << "\r\n" << status() << "\r\n";
            if (g.town_submenu == 4) menu_item("n", "Next page");
            else menu_item("p", "Previous page");
            menu_item("t", "Back");
        }
        break;

    case GameState::Armorer:
        std::cout << "=== The Iron Forge Armory ===\r\n\r\n";
        std::cout << "\"Armor and weapons, forged right here!\"\r\n\r\n";
        if (g.town_submenu == 0) {
            menu_item("1", "Buy Weapon");
            menu_item("2", "Buy Armor");
            menu_item("3", "Sell Equipment (sell for half price)");
            std::cout << "\r\n" << status() << "\r\n";
            menu_item("t", "Return to town");
            menu_item("c", "Go to camp");
        } else if (g.town_submenu == 1) {
            std::cout << "--- Weapons for Sale ---\r\n\r\n";
            int i = 1;
            for (const auto& w : bx::weapons) {
                if (is_weapon_allowed(g.hero.char_class, w.id)) {
                    std::cout << "  [" << i << "] " << w.name
                              << " (" << w.damage_die << ", "
                              << format_price_with_purse(w.cost_gp, g.total_gold) << ")"
                              << (w.two_handed ? " [2H]" : "")
                              << "\r\n";
                    ++i;
                }
            }
            std::cout << "\r\n" << status() << "\r\n";
            menu_item("t", "Back");
        } else if (g.town_submenu == 2) {
            std::cout << "--- Armor for Sale ---\r\n\r\n";
            for (int i = 0; i < 4; ++i) {
                const auto& a = bx::armor_items[i];
                std::string desc = std::string(a.name) + " ("
                    + format_price_with_purse(a.cost_gp, g.total_gold) + ")";
                if (a.ac > 0) desc += " AC " + std::to_string(a.ac);
                else desc += " +1 AC";
                std::cout << "  [" << (i + 1) << "] " << desc << "\r\n";
            }
            std::cout << "\r\n" << status() << "\r\n";
            menu_item("t", "Back");
        } else if (g.town_submenu == 3) {
            std::cout << "--- Sell Equipment ---\r\n\r\n";
            menu_item("1", g.hero.weapon_id.empty() ? "No weapon to sell" :
                        (std::string("Sell weapon (") + g.hero.weapon_id + ")").c_str());
            menu_item("2", g.hero.armor_id.empty() ? "No armor to sell" :
                        (std::string("Sell armor (") + g.hero.armor_id + ")").c_str());
            menu_item("3", g.hero.has_shield ? "Sell shield (5 gp)" : "No shield to sell");
            std::cout << "\r\n" << status() << "\r\n";
            menu_item("t", "Back");
        }
        break;

    case GameState::Camp:
        std::cout << "=== Camp at the Dungeon's Edge ===\r\n\r\n";
        std::cout << "A small clearing with a fire pit. The entrance to\r\n";
        std::cout << "the dungeon looms nearby.\r\n\r\n";
        menu_item("1", "Rest (free — full heal, restore spells)");
        menu_item("2", "Save game");
        menu_item("3", "Enter Dungeon");
        std::cout << "\r\n" << status() << "\r\n";
        menu_item("t", "Return to town");
        menu_item("x", "Quit");
        break;

    case GameState::CharacterSheet:
        std::cout << "=== Character Sheet ===\r\n\r\n";
        std::cout << g.hero.name << " the " << class_name(g.hero.char_class) << "\r\n";
        std::cout << "Level " << g.hero_level << "  XP " << g.total_xp << "  Purse "
                  << format_coin_purse(g.total_gold) << "\r\n";
        std::cout << "Encumbrance " << carried_weight_cn(g) << " cn"
                  << "  Move " << exploration_move_feet(g) << "' ("
                  << encounter_move_feet(g) << "')\r\n";
        std::cout << "HP " << g.hero.hp << "/" << g.hero.max_hp
                  << "  AC " << g.hero.ac
                  << "  Attack +" << g.hero.attack_bonus
                  << "  Damage 1d" << g.hero.damage_die;
        if (g.hero.damage_bonus != 0) std::cout << (g.hero.damage_bonus > 0 ? "+" : "") << g.hero.damage_bonus;
        std::cout << "\r\n\r\n";

        std::cout << "STR " << g.hero.abilities.str
                  << "  DEX " << g.hero.abilities.dex
                  << "  CON " << g.hero.abilities.con
                  << "  INT " << g.hero.abilities.intelligence
                  << "  WIS " << g.hero.abilities.wis
                  << "  CHA " << g.hero.abilities.cha << "\r\n\r\n";

        std::cout << "Weapon: " << (g.hero.weapon_id.empty() ? "none" : g.hero.weapon_id) << "\r\n";
        std::cout << "Armor: " << (g.hero.armor_id.empty() ? "none" : g.hero.armor_id)
                  << (g.hero.has_shield ? " + shield" : "") << "\r\n\r\n";

        if (g.town_submenu == 6) {
            std::cout << "--- Equip Weapon ---\r\n\r\n";
            int shown = 1;
            for (std::size_t i = 0; i < bx::weapons.size(); ++i) {
                if (g.hero.weapon_counts[i] <= 0) continue;
                const auto& w = bx::weapons[i];
                std::cout << "  [" << shown << "] " << w.name << " (" << w.damage_die << ") owned:"
                          << g.hero.weapon_counts[i] << "\r\n";
                ++shown;
            }
            if (shown == 1) std::cout << "  No owned weapons.\r\n";
            menu_item("t", "Back");
        } else if (g.town_submenu == 7) {
            std::cout << "--- Equip Armor / Shield ---\r\n\r\n";
            menu_item("0", "Unequip armor");
            for (std::size_t i = 0; i < bx::armor_items.size(); ++i) {
                if (g.hero.armor_counts[i] <= 0) continue;
                const auto& a = bx::armor_items[i];
                std::cout << "  [" << (i + 1) << "] " << a.name << " owned:"
                          << g.hero.armor_counts[i] << "\r\n";
            }
            menu_item("t", "Back");
        } else {
            std::cout << "--- Inventory ---\r\n";
            for (std::size_t i = 0; i < bx::gear_items.size(); ++i)
                if (g.hero.gear_counts[i] > 0)
                    std::cout << "  " << bx::gear_items[i].name << ": " << g.hero.gear_counts[i] << "\r\n";
            std::cout << "\r\n";
            menu_item("1", "Equip weapon");
            menu_item("2", "Equip armor / shield");
            menu_item("t", "Return");
        }
        break;

    default:
        break;
    }

    int start = std::max(0, static_cast<int>(g.event_log.size()) - 3);
    for (int i = start; i < static_cast<int>(g.event_log.size()); ++i)
        std::cout << "  " << g.event_log[static_cast<std::size_t>(i)] << "\r\n";

    std::cout << "\r\n> " << std::flush;
}

void render(const Game& g) {
    // Town / meta-game screens
    if (g.state != GameState::Dungeon) {
        render_town_screen(g);
        return;
    }

    // ── Dungeon render (original) ──
    std::cout << "\x1B[2J\x1B[H";
    for (int y = 0; y < g.height; ++y) {
        for (int x = 0; x < g.width; ++x) {
            const Tile& t = g.map[index_for(g, x, y)];

            if (!t.explored) {
                std::cout << ' ';
                continue;
            }

            // Dim foreground for explored-but-not-visible tiles
            if (!t.visible)
                std::cout << "\x1B[90m";

            if (t.type == TileType::Wall || is_hidden_secret_door(t)) {
                if (is_visible_wall(g, x, y))
                    std::cout << wall_glyph(g, x, y);
                else
                    std::cout << ' ';
            } else {
                char ch = '.';
                if (t.type == TileType::DoorClosed) ch = '+';
                else if (t.type == TileType::SecretDoorClosed) ch = '+';
                else if (t.type == TileType::DoorOpen) ch = '/';
                else if (t.type == TileType::StairsDown) ch = '>';
                else if (t.type == TileType::StairsUp) ch = '<';

                // Entities only on visible tiles
                if (t.visible) {
                    if (g.hero.alive && g.hero.pos.x == x && g.hero.pos.y == y) {
                        ch = glyph_for(g.hero);
                    } else {
                        for (const auto& m : g.monsters) {
                            if (!m.alive) continue;
                            if (m.pos.x == x && m.pos.y == y) {
                                ch = glyph_for(m);
                                break;
                            }
                        }
                    }
                }

                // If no entity, show items on the ground (dim if explored only)
                if (ch == '.' && !t.items.empty()) {
                    ItemType top = t.items.back();
                    if (top == ItemType::ScrollArcane || top == ItemType::ScrollClerical) {
                        ch = '?';
                    } else if (top == ItemType::Gold) {
                        ch = '$';
                    } else if (top == ItemType::Gem) {
                        ch = '*';
                    } else {
                        ch = '!';
                    }
                }

                // Show visible traps
                if (ch == '.' && t.trap_visible && t.has_trap) {
                    ch = '^';
                }

                std::cout << ch;
            }

            if (!t.visible)
                std::cout << "\x1B[39m";
        }
        std::cout << "\r\n";
    }

    // HUD zone — compact single-line status bar
    int monster_count = 0;
    for (const auto& m : g.monsters)
        if (m.alive) ++monster_count;
    int next_level_xp = xp_for_level(g.hero.char_class, g.hero_level + 1);
    // Compute effective AC from buffs
    int effective_ac = g.hero.ac;
    int buff_turns = 999;
    if (g.hero.shield_turns > 0) {
        effective_ac += 4;
        buff_turns = std::min(buff_turns, g.hero.shield_turns);
    }
    if (g.hero.protection_turns > 0) {
        effective_ac += 1;
        buff_turns = std::min(buff_turns, g.hero.protection_turns);
    }
    if (g.hero.resist_cold_turns > 0) {
        effective_ac += 1;
        buff_turns = std::min(buff_turns, g.hero.resist_cold_turns);
    }
    {
        std::ostringstream oss;
        oss << g.hero.name << "(" << class_name(g.hero.char_class) << ")"
            << " HP:" << g.hero.hp << "/" << g.hero.max_hp
            << " AC:" << g.hero.ac;
        if (effective_ac != g.hero.ac)
            oss << "(" << effective_ac << ":" << buff_turns << ")";
        oss << " Lv." << g.hero_level
            << " D:" << g.dungeon_level
            << " Mv:" << exploration_move_feet(g) << "'"
            << " Purse:" << format_coin_purse(g.total_gold)
            << " XP:" << g.total_xp << "/" << next_level_xp
            << " M:" << monster_count
            << " K:" << g.hero.knives
            << " D:" << g.hero.darts;
        if (g.hero.light_spell_turns > 0)
            oss << " L*:" << g.hero.light_spell_turns;
        else if (g.hero.lantern_turns > 0)
            oss << " Ln:" << g.hero.lantern_turns;
        else
            oss << " T:" << g.hero.torch_turns;
        // Hunger indicator
        if (g.hero.turns_without_food >= 18)
            oss << " STARVING!";
        else if (g.hero.turns_without_food >= 12)
            oss << " Hungry";
        // Confusion indicator
        if (hero_is_confused(g))
            oss << " CONFUSED!";
        // Unconscious indicator
        if (g.hero.unconscious_turns > 0)
            oss << " UNCONSCIOUS!";
        oss << " R:" << g.hero.rations;
        put_line(oss.str());
    }

    // Command hint line — removed; press h for help

    // Event log zone — last 2 events inline
    int start = std::max(0, (int)g.event_log.size() - 2);
    for (int i = start; i < (int)g.event_log.size(); ++i)
        std::cout << "  " << g.event_log[i] << "\r\n";

    // Clear any stale lines then prompt
    std::cout << "\x1B[J> " << std::flush;
}

GameStateSnapshot make_game_state_snapshot(const Game& g) {
    GameStateSnapshot out;
    out.state = g.state;
    out.previous_state = g.previous_state;
    out.town_submenu = g.town_submenu;
    out.width = g.width;
    out.height = g.height;
    out.total_gold = g.total_gold;
    out.total_xp = g.total_xp;
    out.hero_level = g.hero_level;
    out.dungeon_level = g.dungeon_level;
    out.next_level_xp = xp_for_level(g.hero.char_class, g.hero_level + 1);
    out.carried_weight_cn = carried_weight_cn(g);
    out.exploration_move_feet = exploration_move_feet(g);
    out.encounter_move_feet = encounter_move_feet(g);
    out.purse_text = format_coin_purse(g.total_gold);
    out.event_log = g.event_log;

    const std::size_t expected_tiles = (g.width > 0 && g.height > 0)
        ? static_cast<std::size_t>(g.width) * static_cast<std::size_t>(g.height)
        : 0;
    if (expected_tiles > g.map.size()) {
        out.width = 0;
        out.height = 0;
    } else {
        out.tiles.reserve(expected_tiles);
    }
    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            const Tile& t = g.map[index_for(g, x, y)];
            TileView tv;
            tv.type = t.type;
            tv.explored = t.explored;
            tv.visible = t.visible;
            tv.secret_revealed = t.secret_revealed;
            tv.trap_visible = t.trap_visible;
            tv.has_trap = t.has_trap;
            tv.visible_wall = is_visible_wall(g, x, y);
            tv.wall_glyph = wall_glyph(g, x, y);
            tv.items = t.items;
            out.tiles.push_back(std::move(tv));
        }
    }

    const Entity& h = g.hero;
    out.hero.name = h.name;
    out.hero.char_class = h.char_class;
    out.hero.class_name = class_name(h.char_class);
    out.hero.pos = h.pos;
    out.hero.alive = h.alive;
    out.hero.hp = h.hp;
    out.hero.max_hp = h.max_hp;
    out.hero.ac = h.ac;
    out.hero.effective_ac = h.ac;
    out.hero.buff_turns = 999;
    if (h.shield_turns > 0) {
        out.hero.effective_ac += 4;
        out.hero.buff_turns = std::min(out.hero.buff_turns, h.shield_turns);
    }
    if (h.protection_turns > 0) {
        out.hero.effective_ac += 1;
        out.hero.buff_turns = std::min(out.hero.buff_turns, h.protection_turns);
    }
    if (h.resist_cold_turns > 0) {
        out.hero.effective_ac += 1;
        out.hero.buff_turns = std::min(out.hero.buff_turns, h.resist_cold_turns);
    }
    out.hero.attack_bonus = h.attack_bonus;
    out.hero.damage_die = h.damage_die;
    out.hero.damage_bonus = h.damage_bonus;
    out.hero.abilities = h.abilities;
    out.hero.weapon_id = h.weapon_id;
    out.hero.armor_id = h.armor_id;
    out.hero.has_shield = h.has_shield;
    out.hero.weapon_counts = h.weapon_counts;
    out.hero.armor_counts = h.armor_counts;
    out.hero.gear_counts = h.gear_counts;
    out.hero.spells = h.spells;
    out.hero.knives = h.knives;
    out.hero.darts = h.darts;
    out.hero.torch_turns = h.torch_turns;
    out.hero.lantern_turns = h.lantern_turns;
    out.hero.light_spell_turns = h.light_spell_turns;
    out.hero.turns_without_food = h.turns_without_food;
    out.hero.rations = h.rations;
    out.hero.shield_turns = h.shield_turns;
    out.hero.protection_turns = h.protection_turns;
    out.hero.courage_turns = h.courage_turns;
    out.hero.resist_cold_turns = h.resist_cold_turns;
    out.hero.unconscious_turns = h.unconscious_turns;
    out.hero.confused = hero_is_confused(g);

    out.monsters.reserve(g.monsters.size());
    for (const auto& m : g.monsters) {
        EntityView mv;
        mv.name = m.name;
        mv.type = m.type;
        mv.pos = m.pos;
        mv.alive = m.alive;
        mv.glyph = glyph_for(m);
        if (m.alive)
            ++out.alive_monster_count;
        out.monsters.push_back(std::move(mv));
    }

    return out;
}

// ---------------------------
// Monster AI
// ---------------------------
void monsters_turn(Game& g) {
    if (!g.hero.alive) return;

    // Decrement hero buff durations
    if (g.hero.shield_turns > 0)         --g.hero.shield_turns;
    if (g.hero.protection_turns > 0)     --g.hero.protection_turns;
    if (g.hero.courage_turns > 0)        --g.hero.courage_turns;
    if (g.hero.resist_cold_turns > 0)    --g.hero.resist_cold_turns;

    for (auto& m : g.monsters) {
        if (!m.alive) continue;

        // Charmed monsters don't attack the hero
        if (m.charmed) continue;

        // Hero hidden — monsters don't notice the hero
        if (g.hero.hidden) continue;

        if (m.fleeing) {
            flee_ai(g, m);
            continue;
        }

        int dx   = g.hero.pos.x - m.pos.x;
        int dy   = g.hero.pos.y - m.pos.y;
        int dist = std::abs(dx) + std::abs(dy);

        // Adjacent: attack
        if (dist == 1) {
            (void)resolve_first_contact_surprise(g, m);
            if (m.surprised) {
                m.surprised = false;
                log_event(g, "The " + m.name + " loses a moment recovering from surprise.");
                continue;
            }
            // Normal bats never bite; they swarm around the hero's head instead.
            if (is_name_normal_bat(m.name.c_str())) {
                // B/X: normal bats must check morale each round unless
                // controlled or summoned by a magic-user.
                int free_attacks = 0;
                try_morale_check(g, m, g.hero, free_attacks, "surrounding the hero");
                if (!m.alive) continue;
                if (m.fleeing) {
                    flee_ai(g, m);
                    continue;
                }
                if (m.ai_wander_timer++ % 3 == 0) {
                    // Flutter to a random adjacent free tile, keeping the swarm close.
                    static const int dirs[] = {-1, 0, 1, 0, 0, -1, 0, 1};
                    for (int attempt = 0; attempt < 4; ++attempt) {
                        int r = roll_d(g, 4) - 1;
                        int nx = m.pos.x + dirs[r * 2];
                        int ny = m.pos.y + dirs[r * 2 + 1];
                        if (!is_blocked(g, nx, ny) && !is_occupied(g, nx, ny, &m)) {
                            move_entity(g, m, dirs[r * 2], dirs[r * 2 + 1]);
                            break;
                        }
                    }
                }
                continue;
            }
            // A vampire bat next to an unconscious hero feeds without being
            // disturbed, draining 1-4 points of blood per round.
            if (m.is_vampire_bat && g.hero.unconscious_turns > 0) {
                int drain = roll_d(g, 4);
                g.hero.hp -= drain;
                {
                    std::ostringstream oss;
                    oss << "The vampire bat drains " << drain << " HP of blood!";
                    log_event(g, oss.str());
                }
                if (g.hero.hp <= 0) {
                    g.hero.hp = 0;
                    g.hero.alive = false;
                    log_event(g, "The vampire bat drains you dry!");
                    queue_sound(g, SoundEffect::PlayerDeath);
                }
                continue;
            }
            attack(g, m, g.hero);
            if (!g.hero.alive) return;
            continue;
        }

        int step_x = sign(dx);
        int step_y = sign(dy);

        // ── Patrol / wander behavior ──
        // If hero is far away (distance > 8), monsters patrol instead of homing in
        if (dist > 8) {
            // Save spawn position on first patrol
            if (m.ai_patrol_x == 0 && m.ai_patrol_y == 0) {
                m.ai_patrol_x = m.pos.x;
                m.ai_patrol_y = m.pos.y;
            }
            // Wander timer: change direction periodically
            m.ai_wander_timer++;
            if (m.ai_wander_timer >= 8) {
                m.ai_wander_timer = 0;
                // Random direction
                static const int dirs[] = {-1, 0, 1, 0, 0, -1, 0, 1, -1, -1, 1, -1, -1, 1, 1, 1};
                int r = roll_d(g, 8) - 1;
                int rx = dirs[r * 2];
                int ry = dirs[r * 2 + 1];
                int nx = m.pos.x + rx;
                int ny = m.pos.y + ry;
                if (!is_blocked(g, nx, ny) && !is_occupied(g, nx, ny, &m)) {
                    // Tend to stay near patrol point
                    int new_dist = std::abs(nx - m.ai_patrol_x) + std::abs(ny - m.ai_patrol_y);
                    if (new_dist <= 5) {
                        move_entity(g, m, rx, ry);
                    }
                }
            }
            continue;
        }

        // ── Chase behavior (hero within 8 tiles) ──
        if (step_x != 0 &&
            !is_blocked(g, m.pos.x + step_x, m.pos.y) &&
            !is_occupied(g, m.pos.x + step_x, m.pos.y, &m)) {
            move_entity(g, m, step_x, 0);
        } else if (step_y != 0 &&
                   !is_blocked(g, m.pos.x, m.pos.y + step_y) &&
                   !is_occupied(g, m.pos.x, m.pos.y + step_y, &m)) {
            move_entity(g, m, 0, step_y);
        }
    }
}

// ---------------------------
// Ranged attack: trace direction, find target, resolve with range penalty.
// Returns true if a monster was targeted (hit or miss).
static bool ranged_attack(Game& g, Entity& attacker, int dx, int dy,
                          int damage_die, const std::string& weapon_name,
                          int& ammo) {
    if (ammo <= 0) {
        log_event(g, "No " + weapon_name + "s left!");
        return false;
    }

    queue_sound(g, SoundEffect::Throw);

    int dist = 1;
    int nx = attacker.pos.x + dx;
    int ny = attacker.pos.y + dy;

    // Trace until we hit a wall, exit range, or find a monster
    while (in_bounds(g, nx, ny) && dist <= 4) {
        if (is_blocked(g, nx, ny)) {
            break; // wall blocks the throw
        }

        if (Entity* target = monster_at(g, nx, ny)) {
            // Found a target — resolve the attack
            --ammo;
            int roll   = roll_d(g, 20);
            int penalty = (dist >= 4) ? -2 : (dist >= 2) ? -1 : 0;
            // Ranged uses DEX mod instead of STR mod
            int dex_mod = bx::get_dexterity(attacker.abilities.dex).toHitMissile;
            int str_mod = bx::get_strength(attacker.abilities.str).toHitMelee;
            int starvation_penalty = 0;
            if (attacker.type == EntityType::Hero) {
                if (attacker.turns_without_food >= 18) starvation_penalty = -2;
                else if (attacker.turns_without_food >= 12) starvation_penalty = -1;
            }
            int total  = roll + (attacker.attack_bonus - str_mod + dex_mod) + penalty + starvation_penalty;

            {
                std::ostringstream oss;
                oss << attacker.name << " throws " << weapon_name
                    << " at " << target->name
                    << " (d20=" << roll
                    << ", total " << total
                    << " vs AC " << target->ac << ")";
                log_event(g, oss.str());
            }

            if (total >= target->ac) {
                int dmg = roll_d(g, damage_die);
                target->hp -= dmg;

                {
                    std::ostringstream oss;
                    oss << "Hit! " << target->name << " takes "
                        << dmg << " damage.";
                    log_event(g, oss.str());
                }

                if (target->hp <= 0) {
                    target->hp    = 0;
                    target->alive = false;
                    {
                        std::ostringstream oss;
                        oss << target->name << " is slain!";
                        log_event(g, oss.str());
                    }
                    award_treasure(g, *target);
                    check_level_up(g);

                    // Recover the thrown weapon
                    ++ammo;
                    log_event(g, weapon_name + " recovered from "
                               + target->name + ".");
                }
            } else {
                log_event(g, "Miss!");
            }
            return true;
        }

        nx += dx;
        ny += dy;
        ++dist;
    }

    log_event(g, weapon_name + " flies into the darkness.");
    return false;
}

// ---------------------------
// Spellcasting helpers
// ---------------------------

// Returns true if the entity is undead
static bool is_undead(const Entity& e) {
    if (e.type == EntityType::Monster) return e.undead;
    return e.type == EntityType::Skeleton || e.type == EntityType::Zombie;
}

static bool throw_holy_water(Game& g, int dx, int dy) {
    if (dx == 0 && dy == 0) {
        log_event(g, "Throw holy water needs a direction!");
        return false;
    }
    if (g.hero.holy_water <= 0 || !consume_gear(g.hero, "holy-water")) {
        log_event(g, "No holy water left!");
        return false;
    }
    g.hero.holy_water--;
    queue_sound(g, SoundEffect::Throw);

    int dist = 1;
    int nx = g.hero.pos.x + dx;
    int ny = g.hero.pos.y + dy;
    while (in_bounds(g, nx, ny) && dist <= 4) {
        if (is_blocked(g, nx, ny)) break;
        if (Entity* target = monster_at(g, nx, ny)) {
            if (!is_undead(*target)) {
                log_event(g, "The holy water splashes harmlessly.");
                return true;
            }
            int dmg = roll_d(g, 8);
            target->hp -= dmg;
            log_event(g, "Holy water burns " + target->name + " for "
                        + std::to_string(dmg) + " damage!");
            if (target->hp <= 0) {
                target->hp = 0;
                target->alive = false;
                log_event(g, target->name + " is destroyed!");
                award_treasure(g, *target);
                check_level_up(g);
            }
            return true;
        }
        nx += dx;
        ny += dy;
        ++dist;
    }
    log_event(g, "The holy water flask shatters in the darkness.");
    return true;
}

// ---------------------------
// Turn Undead helper
// ---------------------------
static bool turn_undead(Game& g, int dx, int dy) {
    if (dx == 0 && dy == 0) {
        log_event(g, "Turn undead needs a direction!");
        return false;
    }
    if (g.hero.char_class != CharacterClass::Cleric) {
        log_event(g, "Only clerics can turn undead.");
        return false;
    }
    if (!has_gear(g.hero, "holy-symbol")) {
        log_event(g, "You need a holy symbol to turn undead.");
        return false;
    }
    int nx = g.hero.pos.x + dx;
    int ny = g.hero.pos.y + dy;
    if (!in_bounds(g, nx, ny)) {
        log_event(g, "Nothing to turn there.");
        return false;
    }
    Entity* target = monster_at(g, nx, ny);
    if (!target || !is_undead(*target)) {
        log_event(g, "No undead there.");
        return false;
    }
    int roll = roll_2d6(g) + g.hero_level;
    // Simple threshold: HD 1 → 8, HD 2 → 9, higher → harder
    int hd = 1;
    if (target->type == EntityType::Zombie) hd = 2;
    int threshold = 7 + hd;
    {
        std::ostringstream oss;
        oss << "Turn undead: 2d6+" << g.hero_level << " = " << roll
            << " vs " << threshold;
        log_event(g, oss.str());
    }
    if (roll >= threshold + 4) {
        // Destroyed outright
        target->hp    = 0;
        target->alive = false;
        {
            std::ostringstream oss;
            oss << target->name << " is destroyed by the holy power!";
            log_event(g, oss.str());
        }
        award_treasure(g, *target);
        check_level_up(g);
    } else if (roll >= threshold) {
        // Turned — flees
        target->fleeing = true;
        log_event(g, target->name + " flees in terror!");
    } else {
        log_event(g, "Turning fails.");
    }
    return true;
}

// Spawn 1-3 wandering monsters in a random room (not entrance)
// Returns the number of monsters spawned.
static int spawn_wandering_monsters(Game& g) {
    if (monsters_disabled()) return 0;
    if (test_monster_index() >= 0) return 0;
    if (g.rooms.size() <= 1) return 0;

    // Pick a random room that is not the entrance
    int room_idx = roll_d(g, static_cast<int>(g.rooms.size()) - 1); // 1 .. n-1
    const Room& r = g.rooms[room_idx];

    int count = roll_d(g, 3); // 1..3

    for (int i = 0; i < count; ++i) {
        Entity e = spawn_bx_monster(g, g.dungeon_level);

        // Place in the room
        for (int attempt = 0; attempt < 20; ++attempt) {
            int px = r.x + roll_d(g, r.w) - 1;
            int py = r.y + roll_d(g, r.h) - 1;
            if (!is_occupied(g, px, py)) {
                e.pos = {px, py};
                break;
            }
        }
        if (e.pos.x == 0 && e.pos.y == 0)
            e.pos = {r.x + r.w / 2, r.y + r.h / 2};

        g.monsters.push_back(e);
    }

    {
        std::ostringstream oss;
        oss << "Wandering " << (count == 1 ? "monster" : "monsters")
            << " appear in the distance!";
        log_event(g, oss.str());
    }

    return count;
}

// Player command application
// ---------------------------
bool apply_player_command(Game& g, const PlayerCommand& cmd) {
    // A paralyzed or unconscious hero cannot act; the turn still passes for
    // monsters. Exception: casting Cure Light Wounds (which itself cures
    // paralysis) is allowed so the hero can recover in solo play.
    bool cast_exception = (g.hero.paralyzed_turns > 0
                           && cmd.type == PlayerCommandType::Cast);
    if ((g.hero.paralyzed_turns > 0 || g.hero.unconscious_turns > 0)
        && cmd.type != PlayerCommandType::Quit
        && cmd.type != PlayerCommandType::Save
        && !cast_exception) {
        if (g.hero.unconscious_turns > 0) {
            --g.hero.unconscious_turns;
            if (g.hero.unconscious_turns == 0) {
                log_event(g, "You regain consciousness!");
            } else {
                log_event(g, "You are unconscious and cannot act!");
            }
        } else {
            --g.hero.paralyzed_turns;
            if (g.hero.paralyzed_turns == 0) {
                log_event(g, "Your muscles unclench — the paralysis wears off!");
            } else {
                log_event(g, "You are paralyzed and cannot act!");
            }
        }
        return true;
    }

    // Break stealth on most actions (not Wait, Hide, Search, Help, or None)
    if (g.hero.hidden) {
        switch (cmd.type) {
            case PlayerCommandType::Wait:
            case PlayerCommandType::Rest:
            case PlayerCommandType::Hide:
            case PlayerCommandType::Search:
            case PlayerCommandType::Help:
            case PlayerCommandType::Save:
            case PlayerCommandType::None:
                break; // keep hidden
            default:
                g.hero.hidden = false;
                log_event(g, "You emerge from the shadows.");
                break;
        }
    }

    switch (cmd.type) {
        case PlayerCommandType::Quit:
            return false;

        case PlayerCommandType::Save: {
            // Save wherever the hero happens to be, mirroring the camp save.
            // The save does not advance the turn; monsters do not act on it.
            auto save_path =
                save_path_for_hero(g.hero.name.empty() ? "hero" : g.hero.name);
            if (save_game(g, save_path))
                log_event(g, "Game saved to " + save_path.string() + ".");
            else
                log_event(g, "Failed to save game at " + save_path.string() + ".");
            return true;
        }

        case PlayerCommandType::Wait:
            return true;

        case PlayerCommandType::Move: {
            if (hero_move(g, cmd.dx, cmd.dy))
                tick_wander_counter(g);
            return true;
        }

        case PlayerCommandType::Open: {
            // Try to open an adjacent closed door or pick a lock
            static const int dirs[] = {-1, 0, 1, 0, 0, -1, 0, 1};
            bool opened = false;
            for (int i = 0; i < 4; ++i) {
                int nx = g.hero.pos.x + dirs[i * 2];
                int ny = g.hero.pos.y + dirs[i * 2 + 1];
                if (!in_bounds(g, nx, ny)) continue;
                auto& tile = g.map[index_for(g, nx, ny)];
                bool closed_door = tile.type == TileType::DoorClosed
                    || (tile.type == TileType::SecretDoorClosed && tile.secret_revealed);
                if (closed_door && tile.spiked) {
                    log_event(g, "The door is spiked shut.");
                    opened = true;
                    break;
                }
                if (closed_door && tile.held) {
                    log_event(g, "The door is magically held — it won't budge!");
                    opened = true;
                    break;
                }
                if (closed_door && tile.locked) {
                    if (g.hero.char_class == CharacterClass::Thief) {
                        if (!has_gear(g.hero, "thieves-tools")) {
                            log_event(g, "You need thieves' tools to pick locks.");
                            opened = true;
                            break;
                        }
                        int roll = roll_d(g, 100);
                        if (roll <= g.hero.skill_pick_locks) {
                            tile.locked = false;
                            tile.type   = TileType::DoorOpen;
                            log_event(g, "You pick the lock and open the door.");
                            queue_sound(g, SoundEffect::DoorOpen);
                            opened = true;
                            break;
                        } else {
                            log_event(g, "You fail to pick the lock.");
                            opened = true;
                            break;
                        }
                    } else {
                        int od_mod = bx::get_strength(g.hero.abilities.str).openDoors;
                        int roll = roll_d(g, 6);
                        if (roll <= 2 + od_mod) {
                            tile.locked = false;
                            tile.type   = TileType::DoorOpen;
                            queue_sound(g, SoundEffect::DoorOpen);
                            log_event(g, "You force the door open!");
                            opened = true;
                            break;
                        } else {
                            log_event(g, "The door is locked. You fail to force it.");
                            opened = true;
                            break;
                        }
                    }
                }
                if (closed_door) {
                    tile.type = TileType::DoorOpen;
                    queue_sound(g, SoundEffect::DoorOpen);
                    opened = true;
                    log_event(g, "You open a door.");
                    break;
                }
            }
            if (!opened) {
                log_event(g, "Nothing to open.");
            }
            tick_wander_counter(g);
            return true;
        }

        case PlayerCommandType::Close: {
            static const int dirs[] = {-1, 0, 1, 0, 0, -1, 0, 1};
            bool closed = false;
            for (int i = 0; i < 4; ++i) {
                int nx = g.hero.pos.x + dirs[i * 2];
                int ny = g.hero.pos.y + dirs[i * 2 + 1];
                if (!in_bounds(g, nx, ny)) continue;
                auto& tile = g.map[index_for(g, nx, ny)];
                if (tile.type == TileType::DoorOpen && tile.spiked) {
                    log_event(g, "The door is spiked open.");
                    closed = true;
                    break;
                }
                if (tile.type == TileType::DoorOpen) {
                    tile.type = TileType::DoorClosed;
                    queue_sound(g, SoundEffect::DoorClose);
                    closed = true;
                    log_event(g, "You close a door.");
                    break;
                }
            }
            if (!closed) {
                log_event(g, "Nothing to close.");
            }
            tick_wander_counter(g);
            return true;
        }

        case PlayerCommandType::Rest: {
            int heal = roll_d(g, 3); // 1d3
            if (g.hero_level > 1) {
                for (int lvl = 2; lvl <= g.hero_level; ++lvl)
                    heal += roll_d(g, 3);
            }
            int actual = std::min(heal, g.hero.max_hp - g.hero.hp);
            g.hero.hp += actual;

            // Recover spell slots (B-X: full rest restores memorized spells)
            int slots_recovered = 0;
            for (int level = 1; level <= Entity::MaxSpellLevel; ++level) {
                int before = g.hero.spell_slots[level];
                g.hero.spell_slots[level] = g.hero.max_spell_slots[level];
                slots_recovered += g.hero.spell_slots[level] - before;
            }

            {
                std::ostringstream oss;
                oss << "You rest. Heal " << actual << " HP (" << g.hero.hp
                    << "/" << g.hero.max_hp << ").";
                if (slots_recovered > 0)
                    oss << " Spell slots restored.";
                log_event(g, oss.str());
            }

            // B-X: 1-in-6 chance of wandering monsters interrupting rest
            if (roll_d(g, 6) == 1)
                spawn_wandering_monsters(g);

            return true;
        }

        case PlayerCommandType::ThrowKnife: {
            ranged_attack(g, g.hero, cmd.dx, cmd.dy, 4, "knife", g.hero.knives);
            tick_wander_counter(g);
            return true;
        }
        case PlayerCommandType::ThrowDart: {
            ranged_attack(g, g.hero, cmd.dx, cmd.dy, 3, "dart", g.hero.darts);
            tick_wander_counter(g);
            return true;
        }
        case PlayerCommandType::ThrowHolyWater: {
            throw_holy_water(g, cmd.dx, cmd.dy);
            tick_wander_counter(g);
            return true;
        }

        case PlayerCommandType::Cast: {
            cast_spell(g, cmd.spell_index, cmd.dx, cmd.dy);
            tick_wander_counter(g);
            return true;
        }
        case PlayerCommandType::Turn: {
            turn_undead(g, cmd.dx, cmd.dy);
            tick_wander_counter(g);
            return true;
        }

        case PlayerCommandType::Get: {
            auto& hero = g.hero;
            auto& tile = g.map[index_for(g, hero.pos.x, hero.pos.y)];
            int torches = 0;
            int gp_found = 0;
            int gems = 0;
            for (auto it = tile.items.begin(); it != tile.items.end();) {
                if (*it == ItemType::Torch) {
                    hero.torch_turns += 20;
                    it = tile.items.erase(it);
                    ++torches;
                } else if (*it == ItemType::Gold) {
                    int value = 10 + roll_d(g, 20); // 11-30 gp per pile
                    g.total_gold += value;
                    g.total_xp += value;
                    gp_found += value;
                    it = tile.items.erase(it);
                } else if (*it == ItemType::Gem) {
                    int value = 50 + roll_d(g, 100); // 51-150 gp
                    g.total_gold += value;
                    g.total_xp += value; // Gems give XP too
                    gems++;
                    gp_found += value;
                    it = tile.items.erase(it);
                } else {
                    ++it;
                }
            }
            if (torches > 0 || gp_found > 0) {
                if (gp_found > 0) queue_sound(g, SoundEffect::GoldPickup);
                std::ostringstream oss;
                if (torches > 0) {
                    oss << "Picked up " << torches << " torch" << (torches == 1 ? "" : "es") << ".";
                    if (gp_found > 0) oss << " ";
                }
                if (gp_found > 0) {
                    oss << "Found " << gp_found << " gp";
                    if (gems > 0) oss << " (+" << gems << " gem" << (gems == 1 ? "" : "s") << ")";
                    oss << "!";
                }
                log_event(g, oss.str());
            } else {
                log_event(g, "Nothing to pick up here.");
            }
            tick_wander_counter(g);
            return true;
        }

        case PlayerCommandType::Use: {
            auto& tile = g.map[index_for(g, g.hero.pos.x, g.hero.pos.y)];

            // Try to light the lantern (works regardless of ground items)
            if (has_gear(g.hero, "lantern")) {
                if (g.hero.lantern_turns > 0) {
                    log_event(g, "Your lantern is already lit.");
                    tick_wander_counter(g);
                    return true;
                }
                if (!has_gear(g.hero, "tinder-box")) {
                    log_event(g, "You need a tinder box to light the lantern.");
                } else if (g.hero.oil_flasks <= 0 || !consume_gear(g.hero, "oil-flask")) {
                    log_event(g, "You need a flask of oil to fuel the lantern.");
                } else {
                    g.hero.oil_flasks--;
                    g.hero.lantern_turns = 24;
                    log_event(g, "You light the lantern. It will burn for 24 turns.");
                }
                tick_wander_counter(g);
                return true;
            }

            // Light a torch from inventory
            if (g.hero.torch_turns == 0) {
                if (item_count(g.hero, "torches") > 0) {
                    if (!has_gear(g.hero, "tinder-box")) {
                        log_event(g, "You need a tinder box to light a torch.");
                    } else {
                        consume_owned_item(g.hero, "torches");
                        g.hero.torch_turns = 20;
                        log_event(g, "You light a torch.");
                    }
                    tick_wander_counter(g);
                    return true;
                }
            }

            if (tile.items.empty()) {
                static const int dirs[] = {-1, 0, 1, 0, 0, -1, 0, 1};
                for (int i = 0; i < 4; ++i) {
                    int nx = g.hero.pos.x + dirs[i * 2];
                    int ny = g.hero.pos.y + dirs[i * 2 + 1];
                    if (!in_bounds(g, nx, ny)) continue;
                    auto& door = g.map[index_for(g, nx, ny)];
                    if (door.type != TileType::DoorClosed && door.type != TileType::DoorOpen)
                        continue;
                    if (door.spiked) {
                        log_event(g, "That door is already spiked.");
                        tick_wander_counter(g);
                        return true;
                    }
                    if (!has_gear(g.hero, "hammer")) {
                        log_event(g, "You need a hammer to set iron spikes.");
                        tick_wander_counter(g);
                        return true;
                    }
                    if (!consume_gear(g.hero, "iron-spikes")) {
                        log_event(g, "You need iron spikes to spike a door.");
                        tick_wander_counter(g);
                        return true;
                    }
                    door.spiked = true;
                    log_event(g, door.type == TileType::DoorClosed
                        ? "You spike the door shut."
                        : "You spike the door open.");
                    tick_wander_counter(g);
                    return true;
                }
                log_event(g, "Nothing to use here.");
                return true;
            }
            bool used = false;
            for (std::size_t i = 0; i < tile.items.size(); ++i) {
                if (tile.items[i] == ItemType::ScrollArcane) {
                    if (g.hero.char_class != CharacterClass::MagicUser) {
                        log_event(g, "Only Magic-Users can read arcane scrolls.");
                        continue;
                    }
                    std::string spell = (i < tile.scroll_spells.size())
                        ? tile.scroll_spells[i] : "unknown spell";
                    {
                        std::ostringstream oss;
                        oss << "You read the scroll of " << spell
                            << ". It crumbles to dust.";
                        log_event(g, oss.str());
                    }
                    tile.items.erase(tile.items.begin() + static_cast<std::ptrdiff_t>(i));
                    if (i < tile.scroll_spells.size())
                        tile.scroll_spells.erase(tile.scroll_spells.begin() + static_cast<std::ptrdiff_t>(i));
                    used = true;
                    break;
                }
                if (tile.items[i] == ItemType::ScrollClerical) {
                    if (g.hero.char_class != CharacterClass::Cleric) {
                        log_event(g, "Only Clerics can read divine scrolls.");
                        continue;
                    }
                    std::string spell = (i < tile.scroll_spells.size())
                        ? tile.scroll_spells[i] : "unknown spell";
                    {
                        std::ostringstream oss;
                        oss << "You read the scroll of " << spell
                            << ". It crumbles to dust.";
                        log_event(g, oss.str());
                    }
                    tile.items.erase(tile.items.begin() + static_cast<std::ptrdiff_t>(i));
                    if (i < tile.scroll_spells.size())
                        tile.scroll_spells.erase(tile.scroll_spells.begin() + static_cast<std::ptrdiff_t>(i));
                    used = true;
                    break;
                }
            }
            if (!used) {
                log_event(g, "Nothing usable here.");
            }
            tick_wander_counter(g);
            return true;
        }

        case PlayerCommandType::Hide: {
            if (g.hero.char_class != CharacterClass::Thief) {
                log_event(g, "Only Thieves can hide in shadows.");
                return true;
            }
            int roll = roll_d(g, 100);
            if (roll <= g.hero.skill_hide) {
                g.hero.hidden = true;
                log_event(g, "You melt into the shadows.");
            } else {
                log_event(g, "You fail to hide.");
            }
            tick_wander_counter(g);
            return true;
        }

        case PlayerCommandType::Search:
            hero_search(g);
            tick_wander_counter(g);
            return true;

        case PlayerCommandType::Help:
            return true;

        case PlayerCommandType::None:
        default:
            return true;
    }
}

// ── Dungeon entry / exit ──────────────────────────────────

void enter_dungeon(Game& g) {
    g.state = GameState::Dungeon;
    g.event_log.clear();
    g.wander_counter = 0;
    g.tick_clock = 0;
    g.dungeon_peace_announced = false;
    g.monsters.clear();
    regenerate_dungeon(g, 1);
    log_event(g, "You enter the dungeon.");
}

void leave_dungeon(Game& g) {
    g.state = GameState::Camp;
    g.monsters.clear();
    g.map.clear();
    g.rooms.clear();
    g.event_log.clear();
    g.wander_counter = 0;
    g.tick_clock = 0;
}
