#include "game.hpp"
#include "game_snapshot.hpp"
#include "map.hpp"
#include "chargen.hpp"
#include "bx_data.hpp"
#include "item_effects.hpp"

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
    std::uniform_int_distribution<int> dist(1, sides);
    return dist(g.rng);
}

// Check if monster spawning is disabled via environment
static bool monsters_disabled() {
    const char* env = std::getenv("BX_NO_MONSTERS");
    return env && env[0] == '1';
}

// ---------------------------
// Internal helpers
// ---------------------------
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

// Update current_location_id based on hero position.
// For now, just a simple north/south split.
void update_location_description(Game& g) {
    if (g.hero.pos.y < g.height / 2) {
        g.current_location_id = "north_room";
    } else {
        g.current_location_id = "south_room";
    }
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

// Spawn a monster from bx_data. Returns the populated Entity.
static Entity spawn_bx_monster_by_index(Game& g, int idx) {
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

// Forward declarations for morale/attack internals
void try_morale_check(Game& g, Entity& m, Entity& attacker,
                      int& free_attacks_remaining, const std::string& reason);
void morale_allies_on_death(Game& g, const Entity& fallen, Entity& attacker,
                            int& free_attacks_remaining);

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

struct BearAttackProfile {
    int claw_die{};
    int bite_die{};
};

static bool bear_attack_profile(std::string_view name, BearAttackProfile& profile) {
    if (name.rfind("Bear,", 0) != 0) return false;

    if (name.find("Black") != std::string_view::npos) {
        profile = {3, 6};
    } else if (name.find("Cave") != std::string_view::npos) {
        profile = {8, 12};
    } else if (name.find("Grizzly") != std::string_view::npos) {
        profile = {4, 8};
    } else if (name.find("Polar") != std::string_view::npos) {
        profile = {6, 10};
    } else {
        return false;
    }

    return true;
}

static bool resolve_bear_attack(Game& g, Entity& attacker, Entity& defender,
                                int bonus_to_hit) {
    BearAttackProfile profile{};
    if (attacker.type == EntityType::Hero || defender.type != EntityType::Hero
        || !bear_attack_profile(attacker.name, profile)) {
        return false;
    }

    int target = defender.ac;
    if (defender.shield_turns > 0)         target += 4;
    if (defender.protection_turns > 0)     target += 1;
    if (defender.resist_cold_turns > 0)    target += 1;

    bool claw_hits[2] = {false, false};

    auto resolve_one = [&](const char* attack_name, int damage_die, int claw_index) {
        if (!attacker.alive || !defender.alive) return;

        int roll = roll_d(g, 20);
        int total = roll + attacker.attack_bonus + bonus_to_hit;
        {
            std::ostringstream oss;
            oss << attacker.name << " " << attack_name << " attacks " << defender.name
                << " (d20 = " << roll << ", total " << total
                << " vs AC " << target << ")";
            log_event(g, oss.str());
        }

        if (total < target) {
            log_event(g, "Miss!");
            return;
        }

        int dmg = std::max(1, roll_d(g, damage_die));
        defender.hp -= dmg;
        if (claw_index >= 0) claw_hits[claw_index] = true;

        {
            std::ostringstream oss;
            oss << "Hit! " << defender.name << " takes " << dmg << " damage.";
            log_event(g, oss.str());
        }

        if (defender.hp <= 0) {
            defender.hp = 0;
            defender.alive = false;
            queue_sound(g, SoundEffect::PlayerDeath);
            log_event(g, defender.name + " is slain!");
        }
    };

    resolve_one("claw", profile.claw_die, 0);
    resolve_one("claw", profile.claw_die, 1);
    resolve_one("bite", profile.bite_die, -1);

    if (defender.alive && claw_hits[0] && claw_hits[1]) {
        int hug_dmg = roll_d(g, 8) + roll_d(g, 8);
        defender.hp -= hug_dmg;
        {
            std::ostringstream oss;
            oss << attacker.name << " hugs " << defender.name
                << " for " << hug_dmg << " additional damage!";
            log_event(g, oss.str());
        }
        if (defender.hp <= 0) {
            defender.hp = 0;
            defender.alive = false;
            queue_sound(g, SoundEffect::PlayerDeath);
            log_event(g, defender.name + " is slain!");
        }
    }

    return true;
}

// Core attack implementation with optional morale triggering
void attack_internal(Game& g, Entity& attacker, Entity& defender,
                     int bonus_to_hit, bool trigger_morale) {
    if (!attacker.alive || !defender.alive) return;

    if (resolve_bear_attack(g, attacker, defender, bonus_to_hit)) return;

    int roll   = roll_d(g, 20);
    int extra_atk = bonus_to_hit;
    // Remove Fear: +1 attack for the hero
    if (attacker.type == EntityType::Hero && attacker.courage_turns > 0)
        extra_atk += 1;
    // Confused by bats: -2 to hit
    if (attacker.type == EntityType::Hero && hero_is_confused(g))
        extra_atk -= 2;
    // Starvation penalty: -1 to hit when hungry (96+ turns), -2 when starving (144+)
    if (attacker.type == EntityType::Hero) {
        if (attacker.turns_without_food >= 144)
            extra_atk -= 2;
        else if (attacker.turns_without_food >= 96)
            extra_atk -= 1;
    }
    // B/X daylight penalty: light-shy humanoids fight at -1 in the hero's light
    extra_atk -= daylight_to_hit_penalty(g, attacker);
    // Backstab: hidden Thief gets +4 to hit and double damage
    int backstab_bonus = (attacker.type == EntityType::Hero && attacker.hidden) ? 4 : 0;
    int total  = roll + attacker.attack_bonus + extra_atk + backstab_bonus;
    int target = defender.ac; // ascending AC: higher = harder

    // Defensive buffs on the defender
    if (defender.shield_turns > 0)         target += 4;
    if (defender.protection_turns > 0)     target += 1;
    if (defender.resist_cold_turns > 0)    target += 1;

    {
        std::ostringstream oss;
        oss << attacker.name << " attacks " << defender.name
            << " (d20 = " << roll
            << ", total " << total
            << " vs AC " << target << ")";
        log_event(g, oss.str());
    }

    bool defender_is_monster =
        (defender.type != EntityType::Hero) && (defender.morale > 0);

    int free_attacks_remaining = 0;
    if (trigger_morale && attacker.type == EntityType::Hero) {
        free_attacks_remaining = attacker.attacks_per_turn;
    }

    if (total >= target) {
        int dmg = roll_d(g, attacker.damage_die) + attacker.damage_bonus;
        dmg = std::max(1, dmg);
        if (backstab_bonus) {
            dmg *= 2;
            queue_sound(g, SoundEffect::Backstab);
            log_event(g, "Backstab hits for double damage!");
        } else if (attacker.type == EntityType::Hero) {
            queue_sound(g, SoundEffect::AttackHit);
        }
        defender.hp -= dmg;

        {
            std::ostringstream oss;
            oss << "Hit! " << defender.name << " takes "
                << dmg << " damage.";
            log_event(g, oss.str());
        }

        if (defender.hp <= 0) {
            defender.hp    = 0;
            defender.alive = false;

            // Play sound: monster death or hero death
            if (defender.type == EntityType::Hero) {
                queue_sound(g, SoundEffect::PlayerDeath);
            } else {
                queue_sound(g, SoundEffect::MonsterDeath);
            }

            {
                std::ostringstream oss;
                oss << defender.name << " is slain!";
                log_event(g, oss.str());
            }

            // B/X: slaying a lair leader breaks the group's morale.
            if (defender.type != EntityType::Hero && defender.is_leader) {
                std::ostringstream oss;
                oss << "The " << defender.name
                    << " leader has fallen — the group's morale wavers!";
                log_event(g, oss.str());
            }

            // Award treasure + XP when hero kills a monster
            if (attacker.type == EntityType::Hero &&
                defender.type != EntityType::Hero) {
                award_treasure(g, defender);
                check_level_up(g);
            }

            if (trigger_morale && defender_is_monster &&
                attacker.type == EntityType::Hero) {
                morale_allies_on_death(g, defender, attacker,
                                       free_attacks_remaining);
            }
        } else {
            // Poison/disease saves for hero attacked by special monsters
            if (defender.type == EntityType::Hero && attacker.type != EntityType::Hero) {
                if (attacker.has_poison) {
                    if (make_save(g, g.hero.char_class, 0)) {
                        log_event(g, "You resist the poison!");
                    } else {
                        int poison_dmg = roll_d(g, 4);
                        defender.hp -= poison_dmg;
                        {
                            std::ostringstream oss;
                            oss << attacker.name << " poison deals " << poison_dmg << " damage!";
                            log_event(g, oss.str());
                        }
                        if (defender.hp <= 0) {
                            defender.hp = 0;
                            defender.alive = false;
                            log_event(g, "You succumb to the poison!");
                        }
                    }
                } else if (attacker.has_disease) {
                    if (!make_save(g, g.hero.char_class, 0)) {
                        log_event(g, "The " + attacker.name + " infects you with disease!");
                        int disease_dmg = 1;
                        defender.hp -= disease_dmg;
                        if (defender.hp <= 0) {
                            defender.hp = 0;
                            defender.alive = false;
                        }
                    }
                } else if (attacker.has_paralysis) {
                    bool elf_immune = (g.hero.char_class == CharacterClass::Elf
                                       && attacker.name.find("Ghoul") != std::string::npos);
                    if (elf_immune) {
                        log_event(g, "Your elven blood resists the ghoul's paralysis!");
                    } else if (make_save(g, g.hero.char_class, 2)) {
                        log_event(g, "You resist the paralysis!");
                    } else {
                        g.hero.paralyzed_turns = roll_d(g, 4) + roll_d(g, 4);
                        {
                            std::ostringstream oss;
                            oss << attacker.name << " paralyzes you for "
                                << g.hero.paralyzed_turns << " turns!";
                            log_event(g, oss.str());
                        }
                    }
                } else if (attacker.is_vampire_bat) {
                    // B/X: a vampire bat bite does no extra damage, but the
                    // victim must save vs. Paralysis or fall unconscious for
                    // 1-10 rounds, letting the bat drain blood.
                    if (make_save(g, g.hero.char_class, 2)) {
                        log_event(g, "You shake off the vampire bat's bite!");
                    } else {
                        g.hero.unconscious_turns = roll_d(g, 10);
                        {
                            std::ostringstream oss;
                            oss << "The vampire bat's bite knocks you unconscious for "
                                << g.hero.unconscious_turns << " rounds!";
                            log_event(g, oss.str());
                        }
                    }
                }
            }
            if (trigger_morale && defender_is_monster &&
                attacker.type == EntityType::Hero) {
                try_morale_check(g, defender, attacker,
                                 free_attacks_remaining, "is wounded");
            }
        }
    } else {
        log_event(g, "Miss!");
    }

    // No more "Press Enter to continue..." here
}

// Morale: single monster check
void try_morale_check(Game& g, Entity& m, Entity& attacker,
                      int& free_attacks_remaining, const std::string& reason) {
    if (!m.alive) return;
    if (m.type == EntityType::Hero) return;
    int morale = effective_morale(g, m);
    if (m.morale <= 0 && morale <= 0) return;

    int roll = roll_2d6(g);

    {
        std::ostringstream oss;
        oss << m.name << " makes a morale check (" << reason
            << "): 2d6 = " << roll << " vs morale " << morale;
        log_event(g, oss.str());
    }

    if (roll > morale) {
        {
            std::ostringstream oss;
            oss << m.name << " panics and attempts to flee!";
            log_event(g, oss.str());
        }
        m.fleeing = true;

        if (free_attacks_remaining > 0 &&
            attacker.alive &&
            attacker.type == EntityType::Hero) {
            {
                std::ostringstream oss;
                oss << attacker.name
                    << " gains a free attack on the fleeing "
                    << m.name << "!";
                log_event(g, oss.str());
            }
            --free_attacks_remaining;
            attack_internal(g, attacker, m, /*bonus_to_hit=*/2,
                            /*trigger_morale=*/false);

            if (!m.alive) {
                std::ostringstream oss;
                oss << m.name
                    << " is cut down while fleeing!";
                log_event(g, oss.str());
            }
        }
    } else {
        {
            std::ostringstream oss;
            oss << m.name << " holds its ground.";
            log_event(g, oss.str());
        }
    }
}


// Morale: allies of the fallen may break
void morale_allies_on_death(Game& g, const Entity& fallen, Entity& attacker,
                            int& free_attacks_remaining) {
    for (auto& m : g.monsters) {
        if (!m.alive) continue;
        if (m.type != fallen.type) continue;
        if (m.morale <= 0) continue;

        try_morale_check(g, m, attacker, free_attacks_remaining,
                         "an ally dies");
        // free_attacks_remaining is updated inside try_morale_check
    }
}

// Fleeing AI: move away from hero; if at edge, escape the map.
void flee_ai(Game& g, Entity& m) {
    if (!m.alive) return;

    int dx = m.pos.x - g.hero.pos.x;
    int dy = m.pos.y - g.hero.pos.y;

    int step_x = sign(dx);
    int step_y = sign(dy);

    auto try_step = [&](int sx, int sy) {
        if (sx == 0 && sy == 0) return false;
        int nx = m.pos.x + sx;
        int ny = m.pos.y + sy;
        if (!is_blocked(g, nx, ny) && !is_occupied(g, nx, ny, &m)) {
            move_entity(g, m, sx, sy);
            return true;
        }
        return false;
    };

    // Try to move in the direction that increases distance the most
    if (std::abs(dx) >= std::abs(dy)) {
        if (!try_step(step_x, 0)) {
            (void)try_step(0, step_y);
        }
    } else {
        if (!try_step(0, step_y)) {
            (void)try_step(step_x, 0);
        }
    }

    // If they reach the edge of the map, they escape.
    if (m.pos.x == 0 || m.pos.x == g.width - 1 ||
        m.pos.y == 0 || m.pos.y == g.height - 1) {
        m.alive = false;
	std::ostringstream oss;
        oss << m.name << " flees into the darkness!";
        log_event(g, oss.str());
    }
}

// File-local wrappers for attacks used by engine logic
void attack(Game& g, Entity& attacker, Entity& defender) {
    // If the hero attacks a charmed monster, break the charm
    if (attacker.type == EntityType::Hero && defender.charmed) {
        defender.charmed = false;
        log_event(g, defender.name + " breaks free of the charm!");
    }

    // Normal attack: morale + free-attacks logic enabled.
    attack_internal(g, attacker, defender,
                    /*bonus_to_hit=*/0,
                    /*trigger_morale=*/true);
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

// ── Equipment helpers ─────────────────────────────────────

// Parse a B/X damage die string like "1d8" → die size (e.g. 8)
static int parse_die_size(const char* s) {
    if (!s || !*s) return 2;
    std::string_view sv(s);
    auto d = sv.find('d');
    if (d == std::string_view::npos) return 2;
    return std::atoi(sv.data() + d + 1);
}

// Check if a weapon is allowed for a given class using bx_data::class_info.
bool is_weapon_allowed(CharacterClass cc, const char* weapon_id) {
    const bx::ClassInfo* ci = nullptr;
    for (const auto& c : bx::class_info) {
        CharacterClass check_cc = CharacterClass::None;
        if (std::string_view(c.id) == "fighter") check_cc = CharacterClass::Fighter;
        else if (std::string_view(c.id) == "magic-user") check_cc = CharacterClass::MagicUser;
        else if (std::string_view(c.id) == "thief") check_cc = CharacterClass::Thief;
        else if (std::string_view(c.id) == "cleric") check_cc = CharacterClass::Cleric;
        else if (std::string_view(c.id) == "dwarf") check_cc = CharacterClass::Dwarf;
        else if (std::string_view(c.id) == "elf") check_cc = CharacterClass::Elf;
        else if (std::string_view(c.id) == "halfling") check_cc = CharacterClass::Halfling;
        if (check_cc == cc) { ci = &c; break; }
    }
    if (!ci) return false;
    std::string_view allowed(ci->allowed_weapons);
    if (allowed == "any") return true;
    // Build a simplified version without hyphens
    std::string simp;
    for (const char* p = weapon_id; *p; ++p)
        if (*p != '-') simp += *p;
    // Tokenize allowed_weapons by commas and compare case-insensitively
    std::string token;
    for (char ch : allowed) {
        if (ch == ',') {
            // Trim and compare
            while (!token.empty() && token.back() == ' ') token.pop_back();
            while (!token.empty() && token[0] == ' ') token.erase(token.begin());
            if (token.size() == simp.size()) {
                bool match = true;
                for (std::size_t i = 0; i < token.size(); ++i)
                    if (std::tolower(static_cast<unsigned char>(token[i])) !=
                        std::tolower(static_cast<unsigned char>(simp[i]))) {
                        match = false; break;
                    }
                if (match) return true;
            }
            token.clear();
        } else {
            token += ch;
        }
    }
    // Last token
    while (!token.empty() && token.back() == ' ') token.pop_back();
    while (!token.empty() && token[0] == ' ') token.erase(token.begin());
    if (token.size() == simp.size()) {
        bool match = true;
        for (std::size_t i = 0; i < token.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(token[i])) !=
                std::tolower(static_cast<unsigned char>(simp[i]))) {
                match = false; break;
            }
        if (match) return true;
    }
    return false;
}

static bool can_use_shield(CharacterClass cc) {
    for (const auto& c : bx::class_info) {
        CharacterClass check_cc = CharacterClass::None;
        if (std::string_view(c.id) == "fighter") check_cc = CharacterClass::Fighter;
        else if (std::string_view(c.id) == "magic-user") check_cc = CharacterClass::MagicUser;
        else if (std::string_view(c.id) == "thief") check_cc = CharacterClass::Thief;
        else if (std::string_view(c.id) == "cleric") check_cc = CharacterClass::Cleric;
        else if (std::string_view(c.id) == "dwarf") check_cc = CharacterClass::Dwarf;
        else if (std::string_view(c.id) == "elf") check_cc = CharacterClass::Elf;
        else if (std::string_view(c.id) == "halfling") check_cc = CharacterClass::Halfling;
        if (check_cc == cc) return c.allowed_shield;
    }
    return false;
}

static bool can_wear_heavy_armor(CharacterClass cc) {
    for (const auto& c : bx::class_info) {
        CharacterClass check_cc = CharacterClass::None;
        if (std::string_view(c.id) == "fighter") check_cc = CharacterClass::Fighter;
        else if (std::string_view(c.id) == "magic-user") check_cc = CharacterClass::MagicUser;
        else if (std::string_view(c.id) == "thief") check_cc = CharacterClass::Thief;
        else if (std::string_view(c.id) == "cleric") check_cc = CharacterClass::Cleric;
        else if (std::string_view(c.id) == "dwarf") check_cc = CharacterClass::Dwarf;
        else if (std::string_view(c.id) == "elf") check_cc = CharacterClass::Elf;
        else if (std::string_view(c.id) == "halfling") check_cc = CharacterClass::Halfling;
        if (check_cc == cc) return c.allowed_armor_any;
    }
    return false;
}

static void buy_gear_item(Game& g, int gear_index) {
    if (gear_index < 0 || gear_index >= static_cast<int>(bx::gear_items.size())) return;

    const auto& item = bx::gear_items[static_cast<std::size_t>(gear_index)];
    if (g.total_gold < item.cost_gp) {
        log_event(g, std::string("Not enough gold. ") + item.name + " costs "
                    + std::to_string(item.cost_gp) + " gp.");
        return;
    }

    g.total_gold -= item.cost_gp;
    g.hero.gear_counts[static_cast<std::size_t>(gear_index)]++;

    std::string_view id(item.id);
    if (id == "standard-rations") {
        g.hero.rations += 21;
    } else if (id == "iron-rations") {
        g.hero.rations += 42;
    } else if (id == "oil-flask") {
        g.hero.oil_flasks++;
    } else if (id == "holy-water") {
        g.hero.holy_water++;
    } else if (id == "torches") {
        g.hero.torch_turns += 120; // six torches, each worth the current 20-turn torch unit
    }

    log_event(g, std::string("You buy ") + item.name + " for "
                + std::to_string(item.cost_gp) + " gp.");
}

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

int carried_weight_cn(const Game& g) {
    int weight = g.total_gold; // B/X coin-weight: treasure coins count directly.

    for (std::size_t i = 0; i < bx::weapons.size(); ++i)
        weight += g.hero.weapon_counts[i] * bx::weapons[i].weight_cn;
    for (std::size_t i = 0; i < bx::armor_items.size(); ++i)
        weight += g.hero.armor_counts[i] * bx::armor_items[i].weight_cn;
    for (std::size_t i = 0; i < bx::gear_items.size(); ++i)
        weight += g.hero.gear_counts[i] * bx::gear_items[i].weight_cn;

    return weight;
}

int exploration_move_feet(const Game& g) {
    int weight = carried_weight_cn(g);
    if (weight <= 400) return 120;
    if (weight <= 600) return 90;
    if (weight <= 800) return 60;
    return 30;
}

int encounter_move_feet(const Game& g) {
    return exploration_move_feet(g) / 3;
}

int ticks_per_exploration_turn(const Game& g) {
    return exploration_move_feet(g) / 10;
}

static void open_character_sheet(Game& g) {
    if (g.state != GameState::CharacterSheet)
        g.previous_state = g.state;
    g.state = GameState::CharacterSheet;
    g.town_submenu = 0;
}

static void return_from_character_sheet(Game& g) {
    g.state = g.previous_state;
    g.town_submenu = 0;
}

static bool current_weapon_two_handed(const Entity& hero) {
    for (const auto& w : bx::weapons)
        if (w.id == hero.weapon_id) return w.two_handed;
    return false;
}

static void equip_owned_weapon(Game& g, int inventory_index) {
    int shown = 0;
    for (std::size_t wi = 0; wi < bx::weapons.size(); ++wi) {
        if (g.hero.weapon_counts[wi] <= 0) continue;
        if (shown == inventory_index) {
            const auto& w = bx::weapons[wi];
            g.hero.weapon_id = w.id;
            g.hero.damage_die = parse_die_size(w.damage_die);
            if (w.two_handed) g.hero.has_shield = false;
            log_event(g, std::string("Equipped ") + w.name + ".");
            return;
        }
        ++shown;
    }
    log_event(g, "No such owned weapon.");
}

static void equip_owned_armor(Game& g, int armor_index) {
    if (armor_index == -1) {
        g.hero.armor_id.clear();
        g.hero.ac = 9;
        log_event(g, "Unequipped armor.");
        return;
    }
    if (armor_index < 0 || armor_index >= static_cast<int>(bx::armor_items.size())) {
        log_event(g, "No such armor.");
        return;
    }
    if (g.hero.armor_counts[static_cast<std::size_t>(armor_index)] <= 0) {
        log_event(g, "You do not own that armor.");
        return;
    }
    const auto& a = bx::armor_items[static_cast<std::size_t>(armor_index)];
    if (std::string_view(a.id) == "shield") {
        if (g.hero.weapon_id.empty()) {
            log_event(g, "You need a weapon to use a shield.");
        } else if (current_weapon_two_handed(g.hero)) {
            log_event(g, "You can't use a shield with a two-handed weapon.");
        } else {
            g.hero.has_shield = true;
            log_event(g, "Equipped shield.");
        }
        return;
    }
    g.hero.armor_id = a.id;
    g.hero.ac = a.ac;
    log_event(g, std::string("Equipped ") + a.name + ".");
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
void init_game(Game& g) {
    init_game(g, std::filesystem::path{"../data"});
}

void init_game(Game& g, const std::filesystem::path& data_dir) {
    seed_rng(g);
    init_game_with_hero(g, create_hero(g), data_dir, /*should_seed_rng=*/false);
}

void init_game(Game& g, Entity hero) {
    init_game(g, std::move(hero), std::filesystem::path{"../data"});
}

void init_game(Game& g, Entity hero, const std::filesystem::path& data_dir) {
    init_game_with_hero(g, std::move(hero), data_dir, /*should_seed_rng=*/true);
}

void init_game_runtime(Game& g, const std::filesystem::path& data_dir) {
    init_runtime_state(g, data_dir, /*should_seed_rng=*/true);
}

// ---------------------------
// Visibility / Lighting
// ---------------------------

static bool is_opaque(const Tile& t) {
    return t.type == TileType::Wall || t.type == TileType::DoorClosed;
}

static void cast_ray(Game& g, int x0, int y0, int x1, int y1) {
    int adx = std::abs(x1 - x0);
    int ady = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = adx - ady;

    int x = x0, y = y0;
    while (x != x1 || y != y1) {
        int e2 = 2 * err;
        if (e2 > -ady) { err -= ady; x += sx; }
        if (e2 < adx)  { err += adx; y += sy; }

        auto& tile = g.map[index_for(g, x, y)];
        tile.explored = true;

        if (is_opaque(tile) && (x != x1 || y != y1))
            return; // blocked — target not visible
    }
    g.map[index_for(g, x1, y1)].visible = true;
    g.map[index_for(g, x1, y1)].explored = true;
}

void compute_visibility(Game& g) {
    Entity& hero = g.hero;

    // Advance turn clock. At 120' exploration move this is 12 10-foot steps;
    // heavier encumbrance reduces steps per turn per B/X movement rates.
    ++g.tick_clock;
    bool is_turn_tick = (g.tick_clock % ticks_per_exploration_turn(g) == 0);

    if (is_turn_tick) {
        // Check torch state before decrement (for message logic)
        bool torch_was_lit = hero.torch_turns > 0 && hero.light_spell_turns == 0;

        // Decrement light sources once per turn
        if (hero.light_spell_turns > 0)
            --hero.light_spell_turns;
        else if (hero.lantern_turns > 0)
            --hero.lantern_turns;
        else if (hero.torch_turns > 0)
            --hero.torch_turns;

        // Torch status messages (in turns remaining)
        if (torch_was_lit && hero.torch_turns == 0)
            log_event(g, "Your torch sputters out!");
        else if (torch_was_lit && hero.torch_turns == 10)
            log_event(g, "Your torch is burning low.");
        else if (torch_was_lit && hero.torch_turns == 5)
            log_event(g, "Your torch is almost out!");

        // ── Hunger clock (once per B/X turn) ──
        // Every 48 B/X turns (~8 hours), attempt to eat
        hero.turns_without_food++;
        if (hero.turns_without_food > 0 && hero.turns_without_food % 48 == 0) {
            if (hero.rations > 0) {
                hero.rations--;
                hero.turns_without_food = 0;
                log_event(g, "You consume some rations.");
            } else if (hero.turns_without_food == 48) {
                log_event(g, "Your stomach rumbles — you need food.");
            } else if (hero.turns_without_food >= 144 && (hero.turns_without_food / 48) % 3 == 0) {
                // Damage on cycles 144, 288, 432, ... B/X turns
                int dmg = (hero.turns_without_food >= 192) ? roll_d(g, 4) : 1;
                hero.hp -= dmg;
                log_event(g, "You suffer " + std::to_string(dmg) + " damage from starvation!");
                if (hero.hp <= 0) {
                    hero.hp = 0;
                    hero.alive = false;
                    log_event(g, "You have starved to death.");
                }
            }
        }
    }

    // Determine light radius
    int radius = 6;
    if (hero.light_spell_turns > 0)
        radius = 10;
    else if (hero.lantern_turns > 0)
        radius = 6;
    else if (hero.torch_turns <= 0) {
        // No light source: infravision for demihumans
        switch (hero.char_class) {
            case CharacterClass::Dwarf:
            case CharacterClass::Elf:
            case CharacterClass::Halfling:
                radius = 2; // Infravision 60'
                break;
            default:
                radius = 0; // Pitch black
        }
    }

    // Reset visible flags
    for (auto& t : g.map)
        t.visible = false;

    int hx = hero.pos.x, hy = hero.pos.y;
    if (!in_bounds(g, hx, hy)) return;

    // Hero tile always explored and visible
    g.map[index_for(g, hx, hy)].explored = true;
    g.map[index_for(g, hx, hy)].visible = true;

    if (radius <= 0) return;

    // Cast rays to every tile within radius
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx == 0 && dy == 0) continue;
            if (dx * dx + dy * dy > radius * radius) continue;

            int tx = hx + dx;
            int ty = hy + dy;
            if (!in_bounds(g, tx, ty)) continue;
            if (is_opaque(g.map[index_for(g, tx, ty)])) continue;

            cast_ray(g, hx, hy, tx, ty);
        }
    }
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

// Look up spell level from bx_data.hpp by name. Returns 0 if not found.
static int spell_level_by_name(const std::string& name) {
    for (const auto& s : bx::spells) {
        if (name == s.name) return s.level;
    }
    return 0;
}

static bool is_cleric_spell_name(const std::string& name) {
    for (const auto& s : bx::spells) {
        if (name == s.name && std::string_view(s.class_name) == "Cleric")
            return true;
    }
    return false;
}

// Resolve a cast of the spell at spell_index (0-based) toward (dx,dy).
// Returns true if the spell slot was consumed.
static bool cast_spell(Game& g, int spell_index, int dx, int dy) {
    Entity& hero = g.hero;
    if (spell_index < 0 || spell_index >= (int)hero.spells.size()) {
        log_event(g, "That spell is not known.");
        return false;
    }
    const std::string& spell = hero.spells[spell_index];

    // While paralyzed only Cure Light Wounds can be cast (it cures paralysis).
    if (hero.paralyzed_turns > 0 && spell != "Cure Light Wounds") {
        log_event(g, "You are too stiff to cast that spell!");
        return false;
    }

    // An unconscious hero cannot cast spells at all.
    if (hero.unconscious_turns > 0) {
        log_event(g, "You are unconscious and cannot cast spells!");
        return false;
    }

    // Confused by bats: no spellcasting at all.
    if (hero_is_confused(g)) {
        log_event(g, "The bats' fluttering confuses your concentration!");
        return false;
    }

    if (hero.char_class == CharacterClass::Cleric && is_cleric_spell_name(spell)
        && !has_gear(hero, "holy-symbol")) {
        log_event(g, "You need a holy symbol to cast cleric spells.");
        return false;
    }

    int spell_level = spell_level_by_name(spell);
    if (spell_level < 1 || spell_level > Entity::MaxSpellLevel) {
        log_event(g, "Unknown spell level.");
        return false;
    }
    if (hero.spell_slots[spell_level] <= 0) {
        log_event(g, "No spell slots remaining!");
        return false;
    }

    queue_sound(g, SoundEffect::Spell);

    // --- Magic Missile ---
    if (spell == "Magic Missile") {
        if (dx == 0 && dy == 0) {
            log_event(g, "Magic Missile needs a direction!");
            return false;
        }
        int dist = 1;
        int nx = hero.pos.x + dx;
        int ny = hero.pos.y + dy;
        while (in_bounds(g, nx, ny) && dist <= 4) {
            if (is_blocked(g, nx, ny))
                break;
            if (Entity* target = monster_at(g, nx, ny)) {
                --hero.spell_slots[spell_level];
                int dmg = roll_d(g, 4) + 1;
                target->hp -= dmg;
                {
                    std::ostringstream oss;
                    oss << "Magic Missile hits " << target->name
                        << " for " << dmg << " damage!";
                    log_event(g, oss.str());
                }
                if (target->hp <= 0) {
                    target->hp    = 0;
                    target->alive = false;
                    log_event(g, target->name + " is slain!");
                    award_treasure(g, *target);
                    check_level_up(g);
                }
                return true;
            }
            nx += dx;
            ny += dy;
            ++dist;
        }
        log_event(g, "Magic Missile flies into the darkness.");
        return false;
    }

    // --- Sleep ---
    if (spell == "Sleep") {
        --hero.spell_slots[spell_level];
        int hd_affected = roll_d(g, 4) + roll_d(g, 4); // 2d4
        int affected = 0;
        for (auto& m : g.monsters) {
            if (!m.alive) continue;
            if (is_undead(m)) continue; // undead immune
            if (affected >= hd_affected * 2) break; // approx HD
            m.hp    = 0;
            m.alive = false;
            ++affected;
            log_event(g, m.name + " falls asleep!");
        }
        if (affected == 0) {
            log_event(g, "Sleep has no effect.");
        }
        return true;
    }

    // --- Light ---
    if (spell == "Light") {
        --hero.spell_slots[spell_level];
        hero.light_spell_turns = 20;
        log_event(g, "A magical light illuminates the area.");
        return true;
    }

    // --- Cure Light Wounds ---
    if (spell == "Cure Light Wounds") {
        --hero.spell_slots[spell_level];
        int heal = roll_d(g, 6) + 1;
        int actual = std::min(heal, hero.max_hp - hero.hp);
        hero.hp += actual;
        {
            std::ostringstream oss;
            oss << "Cure Light Wounds heals " << actual
                << " HP (" << hero.hp << "/" << hero.max_hp << ").";
            log_event(g, oss.str());
        }
        // Cure Light Wounds also removes paralysis (B/X: "unless removed by a
        // cure light wounds spell").
        if (hero.paralyzed_turns > 0) {
            hero.paralyzed_turns = 0;
            log_event(g, "The spell also cures your paralysis!");
        }
        return true;
    }

    // --- Read Magic ---
    if (spell == "Read Magic") {
        --hero.spell_slots[spell_level];
        log_event(g, "You study the arcane sigils — they become clear.");
        return true;
    }

    // --- Charm Person ---
    if (spell == "Charm Person") {
        if (dx == 0 && dy == 0) {
            log_event(g, "Charm Person needs a direction!");
            return false;
        }
        int nx = hero.pos.x + dx;
        int ny = hero.pos.y + dy;
        Entity* target = monster_at(g, nx, ny);
        if (!target) {
            log_event(g, "No one there to charm.");
            return false;
        }
        if (is_undead(*target)) {
            log_event(g, "Undead are immune to Charm Person.");
            return false;
        }
        --hero.spell_slots[spell_level];
        target->charmed = true;
        log_event(g, target->name + " is charmed!");
        return true;
    }

    // --- Detect Magic ---
    if (spell == "Detect Magic") {
        --hero.spell_slots[spell_level];
        log_event(g, "You detect magical auras in the area.");
        return true;
    }

    // --- Floating Disc ---
    if (spell == "Floating Disc") {
        --hero.spell_slots[spell_level];
        log_event(g, "A floating disc materialises and follows you.");
        return true;
    }

    // --- Hold Portal ---
    if (spell == "Hold Portal") {
        static const int dirs[] = {-1, 0, 1, 0, 0, -1, 0, 1};
        bool held = false;
        for (int i = 0; i < 4; ++i) {
            int nx = hero.pos.x + dirs[i * 2];
            int ny = hero.pos.y + dirs[i * 2 + 1];
            if (!in_bounds(g, nx, ny)) continue;
            auto& tile = g.map[index_for(g, nx, ny)];
            if (tile.type == TileType::DoorClosed) {
                tile.held = true;
                held = true;
                log_event(g, "The door is magically held!");
                break;
            }
        }
        if (!held) {
            log_event(g, "No adjacent door to hold.");
            return false;
        }
        --hero.spell_slots[spell_level];
        return true;
    }

    // --- Protection from Evil ---
    if (spell == "Protection from Evil") {
        --hero.spell_slots[spell_level];
        hero.protection_turns = 6;
        log_event(g, "A protective aura surrounds you.");
        return true;
    }

    // --- Shield ---
    if (spell == "Shield") {
        --hero.spell_slots[spell_level];
        hero.shield_turns = 6;
        log_event(g, "A shimmering shield appears before you.");
        return true;
    }

    // --- Ventriloquism ---
    if (spell == "Ventriloquism") {
        --hero.spell_slots[spell_level];
        log_event(g, "You throw your voice to confuse the monsters.");
        return true;
    }

    // --- Cleric spells ---

    if (spell == "Detect Evil") {
        --hero.spell_slots[spell_level];
        int count = 0;
        for (auto& m : g.monsters)
            if (m.alive) ++count;
        {
            std::ostringstream oss;
            oss << "You detect " << count << " evil presence"
                << (count == 1 ? "" : "s") << " nearby.";
            log_event(g, oss.str());
        }
        return true;
    }

    if (spell == "Purify Food and Water") {
        --hero.spell_slots[spell_level];
        int heal = roll_d(g, 4);
        int actual = std::min(heal, hero.max_hp - hero.hp);
        hero.hp += actual;
        {
            std::ostringstream oss;
            oss << "Purify Food and Water restores " << actual
                << " HP (" << hero.hp << "/" << hero.max_hp << ").";
            log_event(g, oss.str());
        }
        return true;
    }

    if (spell == "Remove Fear") {
        --hero.spell_slots[spell_level];
        hero.courage_turns = 6;
        log_event(g, "Divine courage fills you — +1 to attack rolls.");
        return true;
    }

    if (spell == "Resist Cold") {
        --hero.spell_slots[spell_level];
        hero.resist_cold_turns = 6;
        log_event(g, "A cold-resistant aura surrounds you.");
        return true;
    }

    // --- Level 2 Magic-User spells ---

    if (spell == "Continual Light") {
        --hero.spell_slots[spell_level];
        hero.light_spell_turns = 999;
        log_event(g, "A permanent magical glow fills the area.");
        return true;
    }

    if (spell == "Detect Invisible") {
        --hero.spell_slots[spell_level];
        log_event(g, "You can now see invisible creatures.");
        return true;
    }

    if (spell == "ESP") {
        --hero.spell_slots[spell_level];
        int count = 0;
        for (auto& m : g.monsters)
            if (m.alive) ++count;
        {
            std::ostringstream oss;
            oss << "You sense " << count << " thinking mind"
                << (count == 1 ? "" : "s") << " nearby.";
            log_event(g, oss.str());
        }
        return true;
    }

    if (spell == "Invisibility") {
        --hero.spell_slots[spell_level];
        hero.hidden = true;
        log_event(g, "You fade from sight!");
        return true;
    }

    if (spell == "Knock") {
        --hero.spell_slots[spell_level];
        static const int dirs[] = {-1, 0, 1, 0, 0, -1, 0, 1};
        bool opened = false;
        for (int i = 0; i < 4; ++i) {
            int nx = hero.pos.x + dirs[i * 2];
            int ny = hero.pos.y + dirs[i * 2 + 1];
            if (!in_bounds(g, nx, ny)) continue;
            auto& tile = g.map[index_for(g, nx, ny)];
            if (tile.type == TileType::DoorClosed) {
                tile.type = TileType::DoorOpen;
                tile.held = false;
                opened = true;
                log_event(g, "The door swings open!");
                break;
            }
        }
        if (!opened)
            log_event(g, "No locked door adjacent.");
        return true;
    }

    if (spell == "Levitate") {
        --hero.spell_slots[spell_level];
        log_event(g, "You rise gently into the air.");
        return true;
    }

    if (spell == "Locate Object") {
        --hero.spell_slots[spell_level];
        log_event(g, "You concentrate on a known object...");
        return true;
    }

    if (spell == "Mirror Image") {
        --hero.spell_slots[spell_level];
        log_event(g, "Mirror images of you shimmer into being.");
        return true;
    }

    if (spell == "Phantasmal Force") {
        --hero.spell_slots[spell_level];
        log_event(g, "An illusion shimmers into existence.");
        return true;
    }

    if (spell == "Web") {
        --hero.spell_slots[spell_level];
        int webbed = 0;
        for (auto& m : g.monsters) {
            if (!m.alive) continue;
            m.fleeing = false;
            ++webbed;
        }
        {
            std::ostringstream oss;
            oss << "Thick webs entangle " << webbed << " monster"
                << (webbed == 1 ? "" : "s") << "!";
            log_event(g, oss.str());
        }
        return true;
    }

    if (spell == "Wizard Lock") {
        --hero.spell_slots[spell_level];
        static const int dirs[] = {-1, 0, 1, 0, 0, -1, 0, 1};
        bool locked = false;
        for (int i = 0; i < 4; ++i) {
            int nx = hero.pos.x + dirs[i * 2];
            int ny = hero.pos.y + dirs[i * 2 + 1];
            if (!in_bounds(g, nx, ny)) continue;
            auto& tile = g.map[index_for(g, nx, ny)];
            if (tile.type == TileType::DoorClosed) {
                tile.held = true;
                locked = true;
                log_event(g, "The door is wizard-locked!");
                break;
            }
        }
        if (!locked)
            log_event(g, "No adjacent door to lock.");
        return true;
    }

    // --- Level 2 Cleric spells ---

    if (spell == "Bless") {
        --hero.spell_slots[spell_level];
        int heal = roll_d(g, 4);
        int actual = std::min(heal, hero.max_hp - hero.hp);
        hero.hp += actual;
        hero.courage_turns = std::max(hero.courage_turns, 6);
        {
            std::ostringstream oss;
            oss << "Bless restores " << actual
                << " HP and emboldens you (+1 attack).";
            log_event(g, oss.str());
        }
        return true;
    }

    if (spell == "Hold Person") {
        --hero.spell_slots[spell_level];
        int held = 0;
        for (auto& m : g.monsters) {
            if (!m.alive) continue;
            if (m.morale <= 0) continue;
            m.alive = false;
            ++held;
        }
        {
            std::ostringstream oss;
            oss << "Hold Person paralyzes " << held << " humanoid"
                << (held == 1 ? "" : "s") << "!";
            log_event(g, oss.str());
        }
        return true;
    }

    if (spell == "Silence 15' radius") {
        --hero.spell_slots[spell_level];
        log_event(g, "An eerie silence envelops the area.");
        return true;
    }

    // --- Level 3 Magic-User spells ---

    if (spell == "Dispel Magic") {
        --hero.spell_slots[spell_level];
        // Remove buffs from hero
        hero.shield_turns = 0;
        hero.protection_turns = 0;
        hero.courage_turns = 0;
        hero.resist_cold_turns = 0;
        hero.light_spell_turns = 0;
        // Un-hold nearby doors
        for (auto& tile : g.map)
            tile.held = false;
        log_event(g, "Magic dissipates around you — all enchantments end.");
        return true;
    }

    if (spell == "Fire Ball") {
        --hero.spell_slots[spell_level];
        int killed = 0;
        for (auto& m : g.monsters) {
            if (!m.alive) continue;
            int dmg = roll_d(g, 6) + roll_d(g, 6);
            m.hp -= dmg;
            if (m.hp <= 0) {
                m.hp = 0;
                m.alive = false;
                ++killed;
                log_event(g, m.name + " is consumed by the fire ball!");
            } else {
                log_event(g, m.name + " is scorched by the fire ball!");
            }
        }
        {
            std::ostringstream oss;
            oss << "Fire ball kills " << killed << " monster"
                << (killed == 1 ? "" : "s") << "!";
            log_event(g, oss.str());
        }
        return true;
    }

    if (spell == "Fly") {
        --hero.spell_slots[spell_level];
        log_event(g, "You take to the air!");
        return true;
    }

    // --- Level 1 Magic-User spells not yet implemented ---

    if (spell == "Read Languages") {
        --hero.spell_slots[spell_level];
        log_event(g, "You can read all languages for a short time.");
        return true;
    }

    // Unrecognised spell
    {
        std::ostringstream oss;
        oss << spell << " is not a known spell.";
        log_event(g, oss.str());
    }
    return false;
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

// Trigger a trap on the hero
static void trigger_trap(Game& g, Entity& hero, Tile& tile) {
    if (!tile.has_trap || tile.trap_type == TrapType::None) return;
    queue_sound(g, SoundEffect::Trap);
    int dmg = 0;
    const char* msg = "";
    switch (tile.trap_type) {
        case TrapType::Pit:
            if (consume_gear(hero, "rope")) {
                log_event(g, "Your rope catches fast, sparing you the fall into the pit.");
                tile.has_trap = false;
                tile.trap_type = TrapType::None;
                return;
            }
            dmg = roll_d(g, 6);
            msg = "You fall into a pit!";
            break;
        case TrapType::Dart:
            dmg = roll_d(g, 4);
            msg = "A dart shoots out at you!";
            break;
        case TrapType::PoisonDart:
            dmg = roll_d(g, 4);
            msg = "A poisoned dart hits you!";
            // Poison save
            if (!make_save(g, hero.char_class, 0)) {
                dmg += roll_d(g, 4); // extra poison damage
            }
            break;
        default:
            break;
    }
    if (dmg > 0) {
        hero.hp -= dmg;
        {
            std::ostringstream oss;
            oss << msg << " (" << dmg << " damage)";
            log_event(g, oss.str());
        }
        if (hero.hp <= 0) {
            hero.hp    = 0;
            hero.alive = false;
            queue_sound(g, SoundEffect::PlayerDeath);
            log_event(g, "You have died.");
        }
    }
    tile.has_trap  = false;
    tile.trap_type = TrapType::None;
}

// Advance the wandering monster counter. About every two-thirds of an
// exploration turn, roll 1-in-6 for random encounters.
void tick_wander_counter(Game& g) {
    g.wander_counter++;
    int threshold = std::max(1, (ticks_per_exploration_turn(g) * 2) / 3);
    if (g.wander_counter >= threshold) {
        g.wander_counter = 0;
        if (roll_d(g, 6) == 1)
            spawn_wandering_monsters(g);
    }
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

        case PlayerCommandType::Wait:
            return true;

        case PlayerCommandType::Move: {
            int dx = cmd.dx;
            int dy = cmd.dy;
            if (dx == 0 && dy == 0) return true;

            int nx = g.hero.pos.x + dx;
            int ny = g.hero.pos.y + dy;

            // If a monster is there, attack that monster (with backstab if hidden)
            if (Entity* target = monster_at(g, nx, ny)) {
                bool hero_loses_action = resolve_first_contact_surprise(g, *target);
                if (!hero_loses_action) {
                    attack(g, g.hero, *target);
                    // Backstab breaks stealth
                    if (g.hero.hidden) {
                        g.hero.hidden = false;
                        log_event(g, "You emerge from the shadows.");
                    }
                }
            } else if (in_bounds(g, nx, ny)) {
                Tile& tile = g.map[index_for(g, nx, ny)];

                // Check for trap on destination tile
                if (tile.has_trap && tile.trap_type != TrapType::None && !g.hero.hidden) {
                    // 10-foot pole auto-probes: reveal trap and block movement
                    if (has_gear(g.hero, "pole-10ft")) {
                        tile.trap_visible = true;
                        log_event(g, "Your 10-foot pole reveals a trap ahead!");
                        return true;
                    }
                    trigger_trap(g, g.hero, tile);
                }

                TileType tt = tile.type;
                if (tt == TileType::StairsDown) {
                    descend_stairs(g);
                } else if (tt == TileType::StairsUp) {
                    ascend_stairs(g);
                } else if (tt == TileType::Wall && g.hero.char_class == CharacterClass::Thief) {
                    // Climb walls: Thief can move into wall tiles on success
                    int roll = roll_d(g, 100);
                    if (roll <= g.hero.skill_climb) {
                        log_event(g, "You scale the wall.");
                        g.hero.pos.x = nx;
                        g.hero.pos.y = ny;
                        queue_sound(g, SoundEffect::Footstep);
                        update_location_description(g);
                    } else {
                        log_event(g, "You fail to climb the wall.");
                    }
                } else if (tt != TileType::Wall) {
                    move_entity(g, g.hero, dx, dy);
                    queue_sound(g, SoundEffect::Footstep);
                    update_location_description(g);
                }

                // Move silently check: if hidden, check if monsters nearby notice
                if (g.hero.hidden) {
                    for (auto& m : g.monsters) {
                        if (!m.alive) continue;
                        int dist = std::abs(m.pos.x - g.hero.pos.x) + std::abs(m.pos.y - g.hero.pos.y);
                        if (dist <= 3) {
                            int roll = roll_d(g, 100);
                            if (roll > g.hero.skill_move_silent) {
                                g.hero.hidden = false;
                                log_event(g, "A monster notices you!");
                                break;
                            }
                        }
                    }
                }
            }
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
                if (g.hero.gear_counts[15] > 0) {
                    if (!has_gear(g.hero, "tinder-box")) {
                        log_event(g, "You need a tinder box to light a torch.");
                    } else {
                        g.hero.gear_counts[15]--;
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

        case PlayerCommandType::Search: {
            static const int dirs[] = {-1, 0, 1, 0, 0, -1, 0, 1};
            bool found_anything = false;

            // Search for traps on adjacent tiles
            for (int i = 0; i < 4; ++i) {
                int max_range = has_gear(g.hero, "pole-10ft") ? 2 : 1;
                for (int range = 1; range <= max_range; ++range) {
                    int nx = g.hero.pos.x + dirs[i * 2] * range;
                    int ny = g.hero.pos.y + dirs[i * 2 + 1] * range;
                    if (!in_bounds(g, nx, ny)) continue;
                    auto& tile = g.map[index_for(g, nx, ny)];
                    if (tile.has_trap && tile.trap_type != TrapType::None) {
                        bool found = false;
                        if (has_gear(g.hero, "pole-10ft")) {
                            found = true;
                        } else if (g.hero.char_class == CharacterClass::Thief) {
                            found = (roll_d(g, 100) <= g.hero.skill_find_traps);
                        } else {
                            found = (roll_d(g, 6) == 1); // 1-in-6 for non-thieves
                        }
                        if (found) {
                            tile.trap_visible = true;
                            log_event(g, range == 2
                                ? "Your 10-foot pole reveals a trap ahead!"
                                : "You find a trap!");
                            found_anything = true;
                            // Thief can try to disarm adjacent traps only.
                            if (range == 1 && g.hero.char_class == CharacterClass::Thief) {
                                if (!has_gear(g.hero, "thieves-tools")) {
                                    log_event(g, "You need thieves' tools to disarm traps.");
                                    continue;
                                }
                                int remove_roll = roll_d(g, 100);
                                if (remove_roll <= g.hero.skill_find_traps) {
                                    tile.has_trap   = false;
                                    tile.trap_type  = TrapType::None;
                                    log_event(g, "You skillfully disarm the trap.");
                                } else {
                                    log_event(g, "You fail to disarm the trap.");
                                }
                            }
                        }
                    }
                }
            }

            // Search for adjacent secret doors. B/X baseline is 1-in-6;
            // elves get 1-2 on 1d6. Thieves use their find traps skill.
            for (int i = 0; i < 4; ++i) {
                int nx = g.hero.pos.x + dirs[i * 2];
                int ny = g.hero.pos.y + dirs[i * 2 + 1];
                if (!in_bounds(g, nx, ny)) continue;
                auto& tile = g.map[index_for(g, nx, ny)];
                if (tile.type != TileType::SecretDoorClosed || tile.secret_revealed) continue;

                bool found = false;
                if (g.hero.char_class == CharacterClass::Thief) {
                    found = (roll_d(g, 100) <= g.hero.skill_find_traps);
                } else if (g.hero.char_class == CharacterClass::Elf) {
                    found = (roll_d(g, 6) <= 2);
                } else {
                    found = (roll_d(g, 6) == 1);
                }

                if (found) {
                    tile.secret_revealed = true;
                    found_anything = true;
                    log_event(g, "You find a secret door!");
                }
            }

            // Hear noise at adjacent doors (anyone can try, Thief uses skill)
            for (int i = 0; i < 4; ++i) {
                int nx = g.hero.pos.x + dirs[i * 2];
                int ny = g.hero.pos.y + dirs[i * 2 + 1];
                if (!in_bounds(g, nx, ny)) continue;
                auto& tile = g.map[index_for(g, nx, ny)];
                if (tile.type == TileType::DoorClosed) {
                    bool heard = false;
                    if (g.hero.char_class == CharacterClass::Thief) {
                        heard = (roll_d(g, 100) <= g.hero.skill_hear_noise);
                    } else {
                        heard = (roll_d(g, 6) == 1); // 1-in-6
                    }
                    if (heard) {
                        bool monster_near = false;
                        for (const auto& m : g.monsters) {
                            if (!m.alive) continue;
                            int md_x = std::abs(m.pos.x - nx);
                            int md_y = std::abs(m.pos.y - ny);
                            if (md_x <= 1 && md_y <= 1) {
                                monster_near = true;
                                break;
                            }
                        }
                        if (monster_near) {
                            log_event(g, "You hear something moving behind the door!");
                        } else {
                            log_event(g, "You hear nothing behind the door.");
                        }
                        found_anything = true;
                    }
                }
            }

            if (!found_anything) {
                log_event(g, "You find nothing of interest.");
            }
            tick_wander_counter(g);
            return true;
        }

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

// ── Town command handler ──────────────────────────────────

bool apply_town_command(Game& g, const PlayerCommand& cmd) {
    if (cmd.type == PlayerCommandType::Quit)
        return false;

    if (cmd.type == PlayerCommandType::Inventory) {
        open_character_sheet(g);
        return true;
    }

    auto set_state = [&](GameState s) {
        g.state = s;
        g.town_submenu = 0;
        g.event_log.clear();
    };

    switch (g.state) {
    // ─── Character Sheet ───────────────────────────────────
    case GameState::CharacterSheet:
        if (cmd.type == PlayerCommandType::Help) {
            if (g.town_submenu != 0) g.town_submenu = 0;
            else return_from_character_sheet(g);
            return true;
        }
        if (cmd.type == PlayerCommandType::Get) {
            if (g.town_submenu == 0) {
                if (cmd.spell_index == 0) g.town_submenu = 6;
                else if (cmd.spell_index == 1) g.town_submenu = 7;
            } else if (g.town_submenu == 6) {
                equip_owned_weapon(g, cmd.spell_index);
            } else if (g.town_submenu == 7) {
                equip_owned_armor(g, cmd.spell_index);
            }
            return true;
        }
        return true;

    // ─── Town ──────────────────────────────────────────────
    case GameState::Town:
        switch (cmd.type) {
            case PlayerCommandType::Quit:     return false;
            case PlayerCommandType::Move:
                if      (cmd.dx == 0 && cmd.dy == -1) set_state(GameState::Tavern);
                else if (cmd.dx == -1 && cmd.dy == 0) set_state(GameState::Temple);
                else if (cmd.dx == 1  && cmd.dy == 0) set_state(GameState::Shop);
                else if (cmd.dx == -1 && cmd.dy == 1) set_state(GameState::Armorer);
                else if (cmd.dx == 0 && cmd.dy == 1)  set_state(GameState::Camp);
                break;
            default: break;
        }
        return true;

    // ─── Tavern ────────────────────────────────────────────
    case GameState::Tavern:
        switch (cmd.type) {
            case PlayerCommandType::Rest: {
                if (g.total_gold < 1) {
                    log_event(g, "You don't have a single gold piece to pay for a room.");
                } else {
                    --g.total_gold;
                    int heal = roll_d(g, 3);
                    g.hero.hp += heal;
                    if (g.hero.hp > g.hero.max_hp) g.hero.hp = g.hero.max_hp;
                    log_event(g, "You rent a room for 1 gp and rest, recovering "
                                + std::to_string(heal) + " HP.");
                }
                return true;
            }
            default: break;
        }
        // Navigation from tavern
        if (cmd.type == PlayerCommandType::Move) {
            if (cmd.dx == 2) set_state(GameState::Town);
            else if (cmd.dy == 1) set_state(GameState::Camp);
            return true;
        }
        return true;

    // ─── Temple ────────────────────────────────────────────
    case GameState::Temple:
        switch (cmd.type) {
            case PlayerCommandType::Cast: {
                int cost = 10;
                if (g.total_gold < cost) {
                    log_event(g, "You cannot afford the 10 gp donation for healing.");
                } else if (g.hero.hp >= g.hero.max_hp) {
                    log_event(g, "You are already at full health.");
                } else {
                    g.total_gold -= cost;
                    g.hero.hp += 1;
                    if (g.hero.hp > g.hero.max_hp) g.hero.hp = g.hero.max_hp;
                    log_event(g, "You donate 10 gp — the clerics restore 1 HP.");
                }
                return true;
            }
            case PlayerCommandType::Turn: {
                int amount = std::min(g.total_gold, 100);
                if (amount <= 0) {
                    log_event(g, "You have no gold to donate.");
                } else {
                    g.total_gold -= amount;
                    g.total_xp += amount;
                    log_event(g, "You donate " + std::to_string(amount)
                                + " gp to the temple and gain "
                                + std::to_string(amount) + " XP.");
                }
                return true;
            }
            default: break;
        }
        // Navigation from temple
        if (cmd.type == PlayerCommandType::Move) {
            if (cmd.dx == 2) set_state(GameState::Town);
            else if (cmd.dy == 1) set_state(GameState::Camp);
            return true;
        }
        return true;

    // ─── Shop ──────────────────────────────────────────────
    case GameState::Shop:
        // Back from submenu
        if (cmd.type == PlayerCommandType::Help && g.town_submenu != 0) {
            g.town_submenu = 0;
            return true;
        }
        switch (cmd.type) {
            case PlayerCommandType::Get:
                if (g.town_submenu == 4 || g.town_submenu == 5) {
                    buy_gear_item(g, cmd.spell_index);
                    g.town_submenu = 0;
                }
                return true;
            case PlayerCommandType::Use:
                // Browse B/X gear
                g.town_submenu = 4;
                return true;
            case PlayerCommandType::Open:
                if (g.town_submenu == 4) g.town_submenu = 5;
                return true;
            case PlayerCommandType::Close:
                if (g.town_submenu == 5) g.town_submenu = 4;
                return true;
            default: break;
        }
        // Navigation from shop
        if (cmd.type == PlayerCommandType::Move) {
            if (cmd.dx == 2) set_state(GameState::Town);
            else if (cmd.dy == 1) set_state(GameState::Camp);
            return true;
        }
        return true;

    // ─── Armorer ───────────────────────────────────────────
    case GameState::Armorer:
        // Back from submenu
        if (cmd.type == PlayerCommandType::Help && g.town_submenu != 0) {
            g.town_submenu = 0;
            return true;
        }
        switch (cmd.type) {
            case PlayerCommandType::Get: {
                if (g.town_submenu == 0) {
                    // Option 1: buy weapon -> enters submenu
                    g.town_submenu = 1;
                } else if (g.town_submenu == 1) {
                    // Weapon selection
                    int idx = cmd.spell_index;
                    int count = 0;
                    for (std::size_t wi = 0; wi < bx::weapons.size(); ++wi) {
                        const auto& w = bx::weapons[wi];
                        if (is_weapon_allowed(g.hero.char_class, w.id)) {
                            if (count == idx) {
                                if (g.total_gold >= w.cost_gp) {
                                    g.total_gold -= w.cost_gp;
                                    g.hero.weapon_counts[wi]++;
                                    g.hero.weapon_id = w.id;
                                    g.hero.damage_die = parse_die_size(w.damage_die);
                                    // Two-handed: can't use shield
                                    if (w.two_handed) g.hero.has_shield = false;
                                    log_event(g, std::string("You buy a ") + w.name + " for " + std::to_string(w.cost_gp) + " gp.");
                                } else {
                                    log_event(g, std::string("You can't afford a ") + w.name + " (" + std::to_string(w.cost_gp) + " gp).");
                                }
                                break;
                            }
                            ++count;
                        }
                    }
                    g.town_submenu = 1;
                } else if (g.town_submenu == 2) {
                    // Armor selection
                    int idx = cmd.spell_index;
                    if (idx >= 0 && idx < 4) {
                        const auto& a = bx::armor_items[idx];
                        if (g.total_gold >= a.cost_gp) {
                            // Check class restriction
                            bool allowed = false;
                            if (std::string_view(a.armor_type) == "light") {
                                allowed = true; // light armor allowed for all
                            } else if (std::string_view(a.id) == "shield") {
                                if (can_use_shield(g.hero.char_class)) allowed = true;
                            } else {
                                if (can_wear_heavy_armor(g.hero.char_class)) allowed = true;
                            }
                            if (!allowed) {
                                log_event(g, std::string("Your class cannot wear ") + a.name + ".");
                            } else if (a.ac > 0 && g.hero.ac <= a.ac) {
                                log_event(g, std::string("You already have armor as good as or better than ") + a.name + ".");
                            } else if (std::string_view(a.id) == "shield") {
                                if (g.hero.weapon_id.empty()) {
                                    log_event(g, "You need a weapon to use a shield.");
                                } else {
                                    // Check if current weapon is two-handed
                                    bool two_handed = false;
                                    for (const auto& w : bx::weapons) {
                                        if (w.id == g.hero.weapon_id && w.two_handed) {
                                            two_handed = true; break;
                                        }
                                    }
                                    if (two_handed) {
                                        log_event(g, "You can't use a shield with a two-handed weapon.");
                                    } else {
                                        g.total_gold -= a.cost_gp;
                                        g.hero.armor_counts[static_cast<std::size_t>(idx)]++;
                                        g.hero.has_shield = true;
                                        log_event(g, std::string("You buy a ") + a.name + " for " + std::to_string(a.cost_gp) + " gp.");
                                    }
                                }
                            } else {
                                g.total_gold -= a.cost_gp;
                                g.hero.armor_counts[static_cast<std::size_t>(idx)]++;
                                g.hero.armor_id = a.id;
                                g.hero.ac = a.ac;
                                log_event(g, std::string("You buy ") + a.name + " for " + std::to_string(a.cost_gp) + " gp.");
                            }
                        } else {
                            log_event(g, std::string("You can't afford ") + a.name + " (" + std::to_string(a.cost_gp) + " gp).");
                        }
                    }
                    g.town_submenu = 2;
                } else if (g.town_submenu == 3) {
                    // Sell selection
                    int idx = cmd.spell_index;
                    if (idx == 0 && !g.hero.weapon_id.empty()) {
                        for (const auto& w : bx::weapons) {
                            if (w.id == g.hero.weapon_id) {
                                int price = w.cost_gp / 2;
                                g.total_gold += price;
                                log_event(g, std::string("You sell your ") + w.name + " for " + std::to_string(price) + " gp.");
                                g.hero.weapon_id.clear();
                                g.hero.damage_die = 2; // unarmed: 1d2
                                break;
                            }
                        }
                    } else if (idx == 1 && !g.hero.armor_id.empty()) {
                        for (const auto& a : bx::armor_items) {
                            if (a.id == g.hero.armor_id) {
                                int price = a.cost_gp / 2;
                                g.total_gold += price;
                                log_event(g, std::string("You sell your ") + a.name + " for " + std::to_string(price) + " gp.");
                                g.hero.armor_id.clear();
                                g.hero.ac = 9; // unarmored
                                break;
                            }
                        }
                    } else if (idx == 2 && g.hero.has_shield) {
                        g.total_gold += 5;
                        g.hero.has_shield = false;
                        log_event(g, "You sell your shield for 5 gp.");
                    } else {
                        log_event(g, "Nothing to sell.");
                    }
                    g.town_submenu = 0;
                }
                return true;
            }
            default: break;
        }
        // Navigation from armorer
        if (cmd.type == PlayerCommandType::Move) {
            if (cmd.dx == 2) set_state(GameState::Town);
            else if (cmd.dy == 1) set_state(GameState::Camp);
            return true;
        }
        return true;

    // ─── Camp ──────────────────────────────────────────────
    case GameState::Camp:
        switch (cmd.type) {
            case PlayerCommandType::Rest:
                g.hero.hp = g.hero.max_hp;
                g.hero.torch_turns = 60;
                for (int i = 1; i <= Entity::MaxSpellLevel; ++i)
                    g.hero.spell_slots[i] = g.hero.max_spell_slots[i];
                log_event(g, "You rest at camp and recover fully.");
                return true;
            case PlayerCommandType::Help: {
                auto save_path = save_path_for_hero(g.hero.name.empty() ? "hero" : g.hero.name);
                if (save_game(g, save_path)) {
                    log_event(g, "Game saved to " + save_path.string() + ".");
                }
                else
                    log_event(g, "Failed to save game at " + save_path.string() + ".");
                return true;
            }
            case PlayerCommandType::Move:
                if (cmd.dy == -1) {
                    enter_dungeon(g);
                    return true;
                } else if (cmd.dy == 1) {
                    set_state(GameState::Town);
                    return true;
                }
                break;
            default: break;
        }
        return true;

    default:
        return true;
    }
}
