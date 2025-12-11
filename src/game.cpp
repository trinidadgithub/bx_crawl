#include "game.hpp"
#include "map.hpp"
#include "chargen.hpp"

#include <iostream>
#include <chrono>
#include <cmath>
#include <string>

// ---------------------------
// Random helper
// ---------------------------
int roll_d(Game& g, int sides) {
    std::uniform_int_distribution<int> dist(1, sides);
    return dist(g.rng);
}

// ---------------------------
// Internal helpers (file-local)
// ---------------------------
namespace {

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
    int nx = e.pos.x + dx;
    int ny = e.pos.y + dy;
    if (!is_blocked(g, nx, ny)) {
        e.pos.x = nx;
        e.pos.y = ny;
    }
}

// 2d6 morale roll
int roll_2d6(Game& g) {
    return roll_d(g, 6) + roll_d(g, 6);
}

// Forward decls of morale helpers
void try_morale_check(Game& g, Entity& m, const std::string& reason);
void morale_allies_on_death(Game& g, const Entity& fallen);

// Find the first living monster at (x, y), or nullptr if none.
Entity* monster_at(Game& g, int x, int y) {
    for (auto& m : g.monsters) {
        if (m.alive && m.pos.x == x && m.pos.y == y) {
            return &m;
        }
    }
    return nullptr;
}

// Update current_location_id based on hero position.
// For now, just a very simple split: north vs south.
void update_location_description(Game& g) {
    if (g.hero.pos.y < g.height / 2) {
        g.current_location_id = "north_room";
    } else {
        g.current_location_id = "south_room";
    }
}

// Core attack function with morale hooks
void attack(Game& g, Entity& attacker, Entity& defender) {
    if (!attacker.alive || !defender.alive) return;

    int roll   = roll_d(g, 20);
    int total  = roll + attacker.attack_bonus;
    int target = defender.ac; // ascending AC: higher = harder

    std::cout << attacker.name << " attacks " << defender.name
              << " (d20 = " << roll << ", total " << total
              << " vs AC " << target << ")\n";

    if (total >= target) {
        int dmg = roll_d(g, attacker.damage_die);
        defender.hp -= dmg;
        std::cout << "Hit! " << defender.name << " takes "
                  << dmg << " damage.\n";

        if (defender.hp <= 0) {
            defender.hp    = 0;
            defender.alive = false;
            std::cout << defender.name << " is slain!\n";

            // If a monster dies, its allies may need to check morale.
            if (defender.type != EntityType::Hero && defender.morale > 0) {
                morale_allies_on_death(g, defender);
            }
        } else {
            // Defender survived but is hurt; monsters may check morale.
            if (defender.type != EntityType::Hero && defender.morale > 0) {
                try_morale_check(g, defender, "is wounded");
            }
        }
    } else {
        std::cout << "Miss!\n";
    }

    std::cout << "Press Enter to continue...";
    std::string dummy;
    std::getline(std::cin, dummy);
}

// Morale: single monster check
void try_morale_check(Game& g, Entity& m, const std::string& reason) {
    if (!m.alive) return;
    if (m.type == EntityType::Hero) return;
    if (m.morale <= 0) return;

    int roll = roll_2d6(g);
    std::cout << m.name << " makes a morale check (" << reason
              << "): 2d6 = " << roll << " vs morale " << m.morale << "\n";

    // B/X-style: succeed on <= morale, fail on > morale.
    if (roll > m.morale) {
        std::cout << m.name << " panics and flees!\n";
        m.alive = false; // Treat as having left the battle.
    } else {
        std::cout << m.name << " holds its ground.\n";
    }

    std::cout << "Press Enter to continue...";
    std::string dummy;
    std::getline(std::cin, dummy);
}

// Morale: allies of the fallen may break
void morale_allies_on_death(Game& g, const Entity& fallen) {
    for (auto& m : g.monsters) {
        if (!m.alive) continue;
        if (m.type != fallen.type) continue;
        if (m.morale <= 0) continue;

        try_morale_check(g, m, "an ally dies");
    }
}

} // namespace

// ---------------------------
// Game initialization
// ---------------------------
void init_game(Game& g) {
    g.width  = 20;
    g.height = 10;

    // Seed RNG
    auto seed = static_cast<unsigned long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
    );
    g.rng.seed(seed);

    init_map(g);

    // Load descriptions
    if (!g.descriptions.load_from_file("../data/descriptions.txt")) {
        std::cerr << "Warning: failed to load descriptions.txt\n";
    }

    // Create hero via Fighter-only chargen
    g.hero = create_fighter_hero(g);

    // Startin position in the dungeon
    g.hero.pos = {2, 2};

    g.monsters.clear();

    // Goblin template
    Entity goblin;
    goblin.name         = "Goblin";
    goblin.type         = EntityType::Goblin;
    goblin.max_hp       = 5;
    goblin.hp           = 5;
    goblin.attack_bonus = 1;
    goblin.damage_die   = 6;
    goblin.ac           = 12;
    goblin.morale       = 8;    // reasonably tough
    goblin.alive        = true;

    // Goblin #1
    Entity goblin1 = goblin;
    goblin1.pos = {g.width - 3, g.height - 3};
    g.monsters.push_back(goblin1);

    // Goblin #2
    Entity goblin2 = goblin;
    goblin2.pos = {g.width - 5, 3};
    g.monsters.push_back(goblin2);

    // Kobold template (weaker, lower morale)
    Entity kobold;
    kobold.name         = "Kobold";
    kobold.type         = EntityType::Kobold;
    kobold.max_hp       = 3;
    kobold.hp           = 3;
    kobold.attack_bonus = 0;
    kobold.damage_die   = 4;    // 1d4
    kobold.ac           = 11;
    kobold.morale       = 6;    // more cowardly
    kobold.alive        = true;

    // Kobold #1
    Entity kobold1 = kobold;
    kobold1.pos = {g.width / 2, g.height - 3};
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

    std::cout << "Monsters:\n";
    for (const auto& m : g.monsters) {
        std::cout << " - " << m.name
                  << " HP: " << m.hp << "/" << m.max_hp
                  << (m.alive ? "" : " (gone)")
                  << "  Morale: " << m.morale
                  << "\n";
    }

    std::cout << "\n";

    // Location description:
    if (!g.current_location_id.empty()) {
        std::cout << g.descriptions.get(g.current_location_id) << "\n\n";
    }

    std::cout << "Commands: w/a/s/d to move, q to quit. "
              << "Bump into a monster to attack.\n";
}

// ---------------------------
// Monster AI
// ---------------------------
void monsters_turn(Game& g) {
    if (!g.hero.alive) return;

    for (auto& m : g.monsters) {
        if (!m.alive) continue;

        int dx   = g.hero.pos.x - m.pos.x;
        int dy   = g.hero.pos.y - m.pos.y;
        int dist = std::abs(dx) + std::abs(dy);

        if (dist == 1) {
            attack(g, m, g.hero);
            if (!g.hero.alive) return;
            continue;
        }

        int step_x = sign(dx);
        int step_y = sign(dy);

        if (step_x != 0 && !is_blocked(g, m.pos.x + step_x, m.pos.y)) {
            move_entity(g, m, step_x, 0);
        } else if (step_y != 0 && !is_blocked(g, m.pos.x, m.pos.y + step_y)) {
            move_entity(g, m, 0, step_y);
        }
    }
}

// ---------------------------
// Player input
// ---------------------------
bool handle_player_input(Game& g) {
    std::cout << "> ";
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    if (line.empty()) return true;

    char c = line[0];
    int dx = 0;
    int dy = 0;

    if (c == 'q') {
        return false;
    } else if (c == 'w') {
        dy = -1;
    } else if (c == 's') {
        dy = 1;
    } else if (c == 'a') {
        dx = -1;
    } else if (c == 'd') {
        dx = 1;
    }

    if (dx != 0 || dy != 0) {
        int nx = g.hero.pos.x + dx;
        int ny = g.hero.pos.y + dy;

        if (Entity* target = monster_at(g, nx, ny)) {
            attack(g, g.hero, *target);
        } else {
            move_entity(g, g.hero, dx, dy);
            // Update description after moving
            update_location_description(g);
        }
    }

    return true;
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

