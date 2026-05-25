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
        case EntityType::Goblin: return 10;
        case EntityType::Kobold: return 5;
        default:                 return 5;
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

char glyph_for(const Entity& e) {
    switch (e.type) {
        case EntityType::Hero:   return '@';
        case EntityType::Goblin: return 'g';
        case EntityType::Kobold: return 'k';
        default:                 return '?';
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
    int total  = roll + attacker.attack_bonus + bonus_to_hit;
    int target = defender.ac; // ascending AC: higher = harder

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
    // Normal attack: morale + free-attacks logic enabled.
    attack_internal(g, attacker, defender,
                    /*bonus_to_hit=*/0,
                    /*trigger_morale=*/true);
}

} // end anonymous namespace

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

    // Create hero via Fighter-only chargen
    g.hero = create_fighter_hero(g);

    // Place hero at centre of first room
    if (!rooms.empty()) {
        g.hero.pos = {
            rooms[0].x + rooms[0].w / 2,
            rooms[0].y + rooms[0].h / 2
        };
    } else {
        g.hero.pos = {2, 2};
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

    // Goblin template
    Entity goblin;
    goblin.name             = "Goblin";
    goblin.type             = EntityType::Goblin;
    goblin.max_hp           = 5;
    goblin.hp               = 5;
    goblin.attack_bonus     = 1;
    goblin.damage_die       = 6;
    goblin.ac               = 12;
    goblin.morale           = 8;
    goblin.fleeing          = false;
    goblin.alive            = true;
    goblin.attacks_per_turn = 1;

    Entity goblin1 = goblin;
    goblin1.gold_pieces = roll_d(g, 6) * 10;
    goblin1.pos = random_room_pos(rooms[random_monster_room()]);
    g.monsters.push_back(goblin1);

    Entity goblin2 = goblin;
    goblin2.gold_pieces = roll_d(g, 6) * 10;
    goblin2.pos = random_room_pos(rooms[random_monster_room()]);
    g.monsters.push_back(goblin2);

    // Kobold template
    Entity kobold;
    kobold.name             = "Kobold";
    kobold.type             = EntityType::Kobold;
    kobold.max_hp           = 3;
    kobold.hp               = 3;
    kobold.attack_bonus     = 0;
    kobold.damage_die       = 4;
    kobold.ac               = 11;
    kobold.morale           = 6;
    kobold.fleeing          = false;
    kobold.alive            = true;
    kobold.attacks_per_turn = 1;

    Entity kobold1 = kobold;
    kobold1.gold_pieces = roll_d(g, 6) * 5;
    kobold1.pos = random_room_pos(rooms[random_monster_room()]);
    g.monsters.push_back(kobold1);

    // Initial location description
    g.current_location_id = "start";
}

// ---------------------------
// Rendering
// ---------------------------
void render(const Game& g) {
    // Clear screen (ANSI escape)
    std::cout << "\x1B[2J\x1B[H";

    for (int y = 0; y < g.height; ++y) {
        for (int x = 0; x < g.width; ++x) {
            char ch = ' ';
            const Tile& t = g.map[index_for(g, x, y)];
            if (t.type == TileType::Wall) {
                ch = '#';
            } else {
                ch = '.';
            }

            if (g.hero.alive && g.hero.pos.x == x && g.hero.pos.y == y) {
                ch = glyph_for(g.hero);
            }

            for (const auto& m : g.monsters) {
                if (!m.alive) continue;
                if (m.pos.x == x && m.pos.y == y) {
                    ch = glyph_for(m);
                    break;
                }
            }

            std::cout << ch;
        }
        std::cout << "\n";
    }

    std::cout << "\n";
    std::cout << g.hero.name   << " HP: " << g.hero.hp   << "/" << g.hero.max_hp
              << "  AC: " << g.hero.ac << "\n";

    // Display hero abilities
    int str_mod = ability_modifier_display(g.hero.abilities.str);
    int dex_mod = ability_modifier_display(g.hero.abilities.dex);
    int con_mod = ability_modifier_display(g.hero.abilities.con);

    std::cout << "STR: " << g.hero.abilities.str
              << " (" << (str_mod >= 0 ? "+" : "") << str_mod << ")  ";

    std::cout << "DEX: " << g.hero.abilities.dex
              << " (" << (dex_mod >= 0 ? "+" : "") << dex_mod << ")  ";

    std::cout << "CON: " << g.hero.abilities.con
              << " (" << (con_mod >= 0 ? "+" : "") << con_mod << ")\n";

    int next_level_xp = xp_for_level(g.hero_level + 1);
    std::cout << "Lv." << g.hero_level
              << "  Gold: " << g.total_gold
              << "  XP: " << g.total_xp << "/" << next_level_xp << "\n";

    std::cout << "Monsters:\n";
    for (const auto& m : g.monsters) {
        std::cout << " - " << m.name
                  << " HP: " << m.hp << "/" << m.max_hp
                  << (m.alive ? "" : " (gone)")
                  << "  Morale: " << m.morale
                  << (m.fleeing && m.alive ? " [fleeing]" : "")
                  << "\n";
    }

    std::cout << "\n";

    // Location description:
    if (!g.current_location_id.empty()) {
        std::cout << g.descriptions.get(g.current_location_id) << "\n\n";
    }

    std::cout << "Commands: move with q w e / a s d / z x c (x = quit).\n";
    std::cout << "Bump into a monster to attack.\n";

    std::cout << "\nRecent events:\n";
    for (const auto& msg : g.event_log) {
    	std::cout << "  " << msg << "\n";
    }
}

// ---------------------------
// Monster AI
// ---------------------------
void monsters_turn(Game& g) {
    if (!g.hero.alive) return;

    for (auto& m : g.monsters) {
        if (!m.alive) continue;

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

