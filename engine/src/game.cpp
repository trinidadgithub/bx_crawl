#include "game.hpp"
#include "map.hpp"
#include "chargen.hpp"

#include <iostream>
#include <chrono>
#include <cmath>
#include <string>
#include <sstream>

// ---------------------------
// Random helper
// ---------------------------
int roll_d(Game& g, int sides) {
    std::uniform_int_distribution<int> dist(1, sides);
    return dist(g.rng);
}

// ---------------------------
// Internal helpers
// ---------------------------
namespace {

void log_event(Game& g, const std::string& msg) {
    g.event_log.push_back(msg);
    // keep the log small (e.g., last 8 messages)
    const std::size_t max_events = 8;
    if (g.event_log.size() > max_events) {
        g.event_log.erase(g.event_log.begin(),
                          g.event_log.begin() + (g.event_log.size() - max_events));
    }
}

// Same B/X-ish modifier table as chargen, for display/logging.
int ability_modifier_display(int score) {
    if (score <= 3)  return -3;
    if (score <= 5)  return -2;
    if (score <= 8)  return -1;
    if (score <= 12) return 0;
    if (score <= 15) return 1;
    if (score <= 17) return 2;
    return 3;
}

// Monster XP values (B/X Moldvay)
int monster_xp_value(const Entity& m) {
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

int xp_for_level(int level) {
    if (level >= 9) return 240000 + (level - 9) * 120000;
    if (level == 1) return 0;
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
    if (in_bounds(g, monster.pos.x, monster.pos.y) && roll_d(g, 4) == 1) {
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
    int needed = xp_for_level(g.hero_level + 1);
    while (g.total_xp >= needed && g.hero_level < 14) {
        int con_mod = ability_modifier_display(g.hero.abilities.con);
        int hp_gain = roll_d(g, 8) + con_mod;
        hp_gain = std::max(1, hp_gain);

        g.hero_level++;
        g.hero.max_hp += hp_gain;
        g.hero.hp     += hp_gain;
        g.hero.attack_bonus++;

        {
            std::ostringstream oss;
            oss << g.hero.name << " reaches level " << g.hero_level
                << "! HP +" << hp_gain
                << ", attack bonus now " << g.hero.attack_bonus;
            log_event(g, oss.str());
        }

        needed = xp_for_level(g.hero_level + 1);
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

// Returns true if a tile is carved (passable) — floor or door
static bool is_carved(const Game& g, int x, int y) {
    if (!in_bounds(g, x, y)) return false;
    auto t = g.map[index_for(g, x, y)].type;
    return t == TileType::Floor || t == TileType::DoorClosed || t == TileType::DoorOpen;
}

// A wall tile orthogonally adjacent to a carved tile
static bool is_border_wall(const Game& g, int x, int y) {
    if (!in_bounds(g, x, y)) return false;
    if (g.map[index_for(g, x, y)].type != TileType::Wall) return false;
    return is_carved(g, x, y - 1) || is_carved(g, x, y + 1)
        || is_carved(g, x - 1, y) || is_carved(g, x + 1, y);
}

// A wall tile diagonally adjacent to a carved tile
static bool has_diagonal_carved(const Game& g, int x, int y) {
    if (!in_bounds(g, x, y)) return false;
    if (g.map[index_for(g, x, y)].type != TileType::Wall) return false;
    return is_carved(g, x - 1, y - 1) || is_carved(g, x + 1, y - 1)
        || is_carved(g, x - 1, y + 1) || is_carved(g, x + 1, y + 1);
}

// Whether a wall tile should appear on screen:
//   - border wall (orth carved neighbor), or
//   - diagonal-only tile with at least one border-wall neighbor (corners)
// This limits display to a single layer around carved areas.
static bool is_visible_wall(const Game& g, int x, int y) {
    if (!in_bounds(g, x, y)) return false;
    if (g.map[index_for(g, x, y)].type != TileType::Wall) return false;
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

// Forward declarations for morale/attack internals
void try_morale_check(Game& g, Entity& m, Entity& attacker,
                      int& free_attacks_remaining, const std::string& reason);
void morale_allies_on_death(Game& g, const Entity& fallen, Entity& attacker,
                            int& free_attacks_remaining);

// Core attack implementation with optional morale triggering
void attack_internal(Game& g, Entity& attacker, Entity& defender,
                     int bonus_to_hit, bool trigger_morale) {
    if (!attacker.alive || !defender.alive) return;

    int roll   = roll_d(g, 20);
    int extra_atk = bonus_to_hit;
    // Remove Fear: +1 attack for the hero
    if (attacker.type == EntityType::Hero && attacker.courage_turns > 0)
        extra_atk += 1;
    int total  = roll + attacker.attack_bonus + extra_atk;
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
        int dmg = roll_d(g, attacker.damage_die);
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

            {
                std::ostringstream oss;
                oss << defender.name << " is slain!";
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
    if (m.morale <= 0) return;

    int roll = roll_2d6(g);

    {
        std::ostringstream oss;
        oss << m.name << " makes a morale check (" << reason
            << "): 2d6 = " << roll << " vs morale " << m.morale;
        log_event(g, oss.str());
    }

    if (roll > m.morale) {
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

} // end anonymous namespace

// Class name helper
const char* class_name(CharacterClass cc) {
    switch (cc) {
        case CharacterClass::Fighter:   return "Fighter";
        case CharacterClass::MagicUser: return "Magic-User";
        case CharacterClass::Thief:     return "Thief";
        case CharacterClass::Cleric:    return "Cleric";
        default:                        return "None";
    }
}

// ---------------------------
// Game initialization
// ---------------------------
void init_game(Game& g) {
    g.width  = 40;
    g.height = 20;

    // Seed RNG
    auto seed = static_cast<unsigned long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
    );
    g.rng.seed(seed);

    auto rooms = init_map(g);

    // Load descriptions
    if (!g.descriptions.load_from_file("../data/descriptions.txt")) {
        std::cerr << "Warning: failed to load descriptions.txt\n";
    }

    g.monsters.clear();

    // Create hero with class selection
    g.hero = create_hero(g);

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

    // Monster templates
    struct MonsterTemplate {
        EntityType type;
        std::string name;
        int hp;
        int atk_bonus;
        int damage_die;
        int ac;
        int morale;
        int xp;
    };

    MonsterTemplate templates[] = {
        {EntityType::Goblin,     "Goblin",     5, 1, 6, 12, 8,  10},
        {EntityType::Kobold,     "Kobold",     3, 0, 4, 11, 6,   5},
        {EntityType::GiantRat,   "Giant Rat",  2, 0, 3, 12, 8,   5},
        {EntityType::Skeleton,   "Skeleton",   4, 0, 6, 12, 12, 10},
        {EntityType::Orc,        "Orc",        5, 1, 6, 13, 8,  10},
        {EntityType::GiantSpider,"Giant Spider",4, 0, 6, 11, 8,  13},
        {EntityType::Zombie,     "Zombie",     9, 1, 6, 11, 12,  20},
    };
    const int num_types = 7;

    // Spawn 4-6 monsters
    int num_monsters = 4 + roll_d(g, 3); // 4..6
    for (int i = 0; i < num_monsters; ++i) {
        const auto& t = templates[roll_d(g, num_types) - 1];

        Entity e;
        e.name             = t.name;
        e.type             = t.type;
        e.max_hp           = t.hp;
        e.hp               = t.hp;
        e.attack_bonus     = t.atk_bonus;
        e.damage_die       = t.damage_die;
        e.ac               = t.ac;
        e.morale           = t.morale;
        e.fleeing          = false;
        e.alive            = true;
        e.attacks_per_turn = 1;

        // Gold by type
        switch (t.type) {
            case EntityType::Goblin:
            case EntityType::Orc:
                e.gold_pieces = roll_d(g, 6) * 10;
                break;
            case EntityType::Kobold:
                e.gold_pieces = roll_d(g, 6) * 5;
                break;
            default:
                e.gold_pieces = 0;
                break;
        }

        place_monster(e, rooms[random_monster_room()]);
        g.monsters.push_back(e);
    }

    // Initial location description
    g.current_location_id = "start";
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

    // Decrement light sources
    if (hero.light_spell_turns > 0)
        --hero.light_spell_turns;
    else if (hero.torch_turns > 0)
        --hero.torch_turns;

    // Determine light radius
    int radius = 6;
    if (hero.light_spell_turns > 0)
        radius = 10;
    else if (hero.torch_turns <= 0)
        radius = 0;

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

void render(const Game& g) {
    // Clear entire screen so we always start at a known scroll position
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

            if (t.type == TileType::Wall) {
                if (is_visible_wall(g, x, y))
                    std::cout << wall_glyph(g, x, y);
                else
                    std::cout << ' ';
            } else {
                char ch = '.';
                if (t.type == TileType::DoorClosed) ch = '+';
                else if (t.type == TileType::DoorOpen) ch = '/';

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
                    ch = '!';
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
    int next_level_xp = xp_for_level(g.hero_level + 1);
    {
        std::ostringstream oss;
        oss << g.hero.name << "(" << class_name(g.hero.char_class) << ")"
            << " HP:" << g.hero.hp << "/" << g.hero.max_hp
            << " AC:" << g.hero.ac
            << " Lv." << g.hero_level
            << " G:" << g.total_gold
            << " XP:" << g.total_xp << "/" << next_level_xp
            << " M:" << monster_count
            << " K:" << g.hero.knives
            << " D:" << g.hero.darts
            << " T:" << g.hero.torch_turns;
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

        if (m.fleeing) {
            flee_ai(g, m);
            continue;
        }

        int dx   = g.hero.pos.x - m.pos.x;
        int dy   = g.hero.pos.y - m.pos.y;
        int dist = std::abs(dx) + std::abs(dy);

        // Simple “Manhattan distance” 1 = adjacent (orthogonal)
        if (dist == 1) {
            attack(g, m, g.hero);
            if (!g.hero.alive) return;
            continue;
        }

        int step_x = sign(dx);
        int step_y = sign(dy);

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

    int dist = 1;
    int nx = attacker.pos.x + dx;
    int ny = attacker.pos.y + dy;

    // Trace until we hit a wall, exit range, or find a monster
    while (in_bounds(g, nx, ny) && dist <= 4) {
        if (is_blocked(g, nx, ny) && g.map[index_for(g, nx, ny)].type != TileType::DoorClosed) {
            break; // wall blocks the throw
        }

        if (Entity* target = monster_at(g, nx, ny)) {
            // Found a target — resolve the attack
            --ammo;
            int roll   = roll_d(g, 20);
            int penalty = (dist >= 4) ? -2 : (dist >= 2) ? -1 : 0;
            int total  = roll + attacker.attack_bonus + penalty;

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

// Returns true if the name is an undead type
static bool is_undead(EntityType type) {
    return type == EntityType::Skeleton || type == EntityType::Zombie;
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

    // Check spell slot availability (level 1 slots for now)
    if (hero.spell_slots[1] <= 0) {
        log_event(g, "No spell slots remaining!");
        return false;
    }

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
            if (is_blocked(g, nx, ny) &&
                g.map[index_for(g, nx, ny)].type != TileType::DoorClosed)
                break;
            if (Entity* target = monster_at(g, nx, ny)) {
                --hero.spell_slots[1];
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
        --hero.spell_slots[1];
        int hd_affected = roll_d(g, 4) + roll_d(g, 4); // 2d4
        int affected = 0;
        for (auto& m : g.monsters) {
            if (!m.alive) continue;
            if (is_undead(m.type)) continue; // undead immune
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
        --hero.spell_slots[1];
        hero.light_spell_turns = 20;
        log_event(g, "A magical light illuminates the area.");
        return true;
    }

    // --- Cure Light Wounds ---
    if (spell == "Cure Light Wounds") {
        --hero.spell_slots[1];
        int heal = roll_d(g, 6) + 1;
        int actual = std::min(heal, hero.max_hp - hero.hp);
        hero.hp += actual;
        {
            std::ostringstream oss;
            oss << "Cure Light Wounds heals " << actual
                << " HP (" << hero.hp << "/" << hero.max_hp << ").";
            log_event(g, oss.str());
        }
        return true;
    }

    // --- Read Magic ---
    if (spell == "Read Magic") {
        --hero.spell_slots[1];
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
        if (is_undead(target->type)) {
            log_event(g, "Undead are immune to Charm Person.");
            return false;
        }
        --hero.spell_slots[1];
        target->charmed = true;
        log_event(g, target->name + " is charmed!");
        return true;
    }

    // --- Detect Magic ---
    if (spell == "Detect Magic") {
        --hero.spell_slots[1];
        log_event(g, "You detect magical auras in the area.");
        return true;
    }

    // --- Floating Disc ---
    if (spell == "Floating Disc") {
        --hero.spell_slots[1];
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
        --hero.spell_slots[1];
        return true;
    }

    // --- Protection from Evil ---
    if (spell == "Protection from Evil") {
        --hero.spell_slots[1];
        hero.protection_turns = 6;
        log_event(g, "A protective aura surrounds you.");
        return true;
    }

    // --- Shield ---
    if (spell == "Shield") {
        --hero.spell_slots[1];
        hero.shield_turns = 6;
        log_event(g, "A shimmering shield appears before you.");
        return true;
    }

    // --- Ventriloquism ---
    if (spell == "Ventriloquism") {
        --hero.spell_slots[1];
        log_event(g, "You throw your voice to confuse the monsters.");
        return true;
    }

    // --- Cleric spells ---

    if (spell == "Detect Evil") {
        --hero.spell_slots[1];
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
        --hero.spell_slots[1];
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
        --hero.spell_slots[1];
        hero.courage_turns = 6;
        log_event(g, "Divine courage fills you — +1 to attack rolls.");
        return true;
    }

    if (spell == "Resist Cold") {
        --hero.spell_slots[1];
        hero.resist_cold_turns = 6;
        log_event(g, "A cold-resistant aura surrounds you.");
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
    int nx = g.hero.pos.x + dx;
    int ny = g.hero.pos.y + dy;
    if (!in_bounds(g, nx, ny)) {
        log_event(g, "Nothing to turn there.");
        return false;
    }
    Entity* target = monster_at(g, nx, ny);
    if (!target || !is_undead(target->type)) {
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

// Player command application
// ---------------------------
bool apply_player_command(Game& g, const PlayerCommand& cmd) {
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

            // If a monster is there, attack that monster
            if (Entity* target = monster_at(g, nx, ny)) {
                attack(g, g.hero, *target);
            } else {
                move_entity(g, g.hero, dx, dy);
                update_location_description(g);
            }
            return true;
        }

        case PlayerCommandType::Open: {
            // Try to open an adjacent closed door
            static const int dirs[] = {-1, 0, 1, 0, 0, -1, 0, 1};
            bool opened = false;
            for (int i = 0; i < 4; ++i) {
                int nx = g.hero.pos.x + dirs[i * 2];
                int ny = g.hero.pos.y + dirs[i * 2 + 1];
                if (!in_bounds(g, nx, ny)) continue;
                auto& tile = g.map[index_for(g, nx, ny)];
                if (tile.type == TileType::DoorClosed && tile.held) {
                    log_event(g, "The door is magically held — it won't budge!");
                    opened = true;
                    break;
                }
                if (tile.type == TileType::DoorClosed) {
                    tile.type = TileType::DoorOpen;
                    opened = true;
                    log_event(g, "You open a door.");
                    break;
                }
            }
            if (!opened) {
                log_event(g, "Nothing to open.");
            }
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
                if (tile.type == TileType::DoorOpen) {
                    tile.type = TileType::DoorClosed;
                    closed = true;
                    log_event(g, "You close a door.");
                    break;
                }
            }
            if (!closed) {
                log_event(g, "Nothing to close.");
            }
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
            {
                std::ostringstream oss;
                oss << "You rest. Heal " << actual << " HP (" << g.hero.hp
                    << "/" << g.hero.max_hp << ").";
                log_event(g, oss.str());
            }
            return true;
        }

        case PlayerCommandType::ThrowKnife: {
            ranged_attack(g, g.hero, cmd.dx, cmd.dy, 4, "knife", g.hero.knives);
            return true;
        }
        case PlayerCommandType::ThrowDart: {
            ranged_attack(g, g.hero, cmd.dx, cmd.dy, 3, "dart", g.hero.darts);
            return true;
        }

        case PlayerCommandType::Cast: {
            cast_spell(g, cmd.spell_index, cmd.dx, cmd.dy);
            return true;
        }
        case PlayerCommandType::Turn: {
            turn_undead(g, cmd.dx, cmd.dy);
            return true;
        }

        case PlayerCommandType::Get: {
            auto& hero = g.hero;
            auto& tile = g.map[index_for(g, hero.pos.x, hero.pos.y)];
            int picked = 0;
            for (auto it = tile.items.begin(); it != tile.items.end();) {
                if (*it == ItemType::Torch) {
                    hero.torch_turns += 20;
                    it = tile.items.erase(it);
                    ++picked;
                } else {
                    ++it;
                }
            }
            if (picked > 0) {
                std::ostringstream oss;
                oss << "Picked up " << picked << " torch" << (picked == 1 ? "" : "es") << ".";
                log_event(g, oss.str());
            } else {
                log_event(g, "Nothing to pick up here.");
            }
            return true;
        }

        case PlayerCommandType::Help:
            return true;

        case PlayerCommandType::None:
        default:
            return true;
    }
}

// ---------------------------
// Initiative
// ---------------------------
bool roll_initiative(Game& g) {
    int hero_roll     = roll_d(g, 6);
    int monster_roll  = roll_d(g, 6);

    // Re-roll ties
    while (hero_roll == monster_roll) {
        hero_roll    = roll_d(g, 6);
        monster_roll = roll_d(g, 6);
    }

    {
        std::ostringstream oss;
        oss << "Initiative: Hero " << hero_roll
            << " / Monsters " << monster_roll
            << " — " << (monster_roll > hero_roll ? "Monsters" : "Hero") << " first";
        log_event(g, oss.str());
    }

    return monster_roll > hero_roll;
}

// ---------------------------
// Utility: monsters alive?
// ---------------------------
bool any_monsters_alive(const Game& g) {
    for (const auto& m : g.monsters) {
        if (m.alive) return true;
    }
    return false;
}

