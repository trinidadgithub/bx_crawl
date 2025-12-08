#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <cmath>

// ---------------------------
// Basic types and utilities
// ---------------------------
struct Vec2 {
    int x{};
    int y{};
};

enum class TileType {
    Floor,
    Wall
};

struct Tile {
    TileType type = TileType::Floor;
};

enum class EntityType {
    Hero,
    Goblin
};

struct Entity {
    std::string name;
    EntityType type;
    Vec2 pos;
    int max_hp{};
    int hp{};
    int attack_bonus{}; // simple +to-hit
    int damage_die{};   // e.g. 6 for 1d6
    int ac{};           // ascending AC for simplicity (higher = harder to hit)
    bool alive{true};
};

struct Game {
    int width{};
    int height{};
    std::vector<Tile> map;
    Entity hero;
    Entity goblin;
    std::mt19937 rng;
};

int idx(const Game& g, int x, int y) {
    return y * g.width + x;
}

bool in_bounds(const Game& g, int x, int y) {
    return x >= 0 && x < g.width && y >= 0 && y < g.height;
}

int roll_d(Game& g, int sides) {
    std::uniform_int_distribution<int> dist(1, sides);
    return dist(g.rng);
}

// ---------------------------
// Map and game initialization
// ---------------------------
void init_map(Game& g) {
    g.map.resize(g.width * g.height);

    // Simple room: walls around the edges, floor inside.
    for (int y = 0; y < g.height; ++y) {
        for (int x = 0; x < g.width; ++x) {
            Tile& t = g.map[idx(g, x, y)];
            if (x == 0 || y == 0 || x == g.width - 1 || y == g.height - 1) {
                t.type = TileType::Wall;
            } else {
                t.type = TileType::Floor;
            }
        }
    }
}

void init_game(Game& g) {
    g.width  = 20;
    g.height = 10;

    // Seed RNG
    auto seed = static_cast<unsigned long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
    );
    g.rng.seed(seed);

    init_map(g);

    // Hero: B/X-ish fighter-ish numbers, but simplified
    g.hero.name         = "Hero";
    g.hero.type         = EntityType::Hero;
    g.hero.pos          = {2, 2};
    g.hero.max_hp       = 8;   // 1d8-ish
    g.hero.hp           = 8;
    g.hero.attack_bonus = 2;   // small +to-hit
    g.hero.damage_die   = 8;   // 1d8
    g.hero.ac           = 14;  // ascending AC
    g.hero.alive        = true;

    // Goblin
    g.goblin.name         = "Goblin";
    g.goblin.type         = EntityType::Goblin;
    g.goblin.pos          = {g.width - 3, g.height - 3};
    g.goblin.max_hp       = 5;
    g.goblin.hp           = 5;
    g.goblin.attack_bonus = 1;
    g.goblin.damage_die   = 6;  // 1d6
    g.goblin.ac           = 12;
    g.goblin.alive        = true;
}

// ---------------------------
// Rendering
// ---------------------------
void render(const Game& g) {
    // Clear screen (portable-ish fallback)
    std::cout << "\x1B[2J\x1B[H";

    // Draw map
    for (int y = 0; y < g.height; ++y) {
        for (int x = 0; x < g.width; ++x) {
            char ch = ' ';
            const Tile& t = g.map[idx(g, x, y)];
            if (t.type == TileType::Wall) {
                ch = '#';
            } else {
                ch = '.';
            }

            // Draw entities on top
            if (g.hero.alive && g.hero.pos.x == x && g.hero.pos.y == y) {
                ch = '@';
            }
            if (g.goblin.alive && g.goblin.pos.x == x && g.goblin.pos.y == y) {
                ch = 'g';
            }

            std::cout << ch;
        }
        std::cout << "\n";
    }

    std::cout << "\n";
    std::cout << g.hero.name   << " HP: " << g.hero.hp   << "/" << g.hero.max_hp
              << "  AC: " << g.hero.ac << "\n";
    std::cout << g.goblin.name << " HP: " << g.goblin.hp << "/" << g.goblin.max_hp
              << "  AC: " << g.goblin.ac << "\n";

    std::cout << "\nCommands: w/a/s/d to move, q to quit. "
              << "Bump into the goblin to attack.\n";
}

// ---------------------------
// Combat
// ---------------------------
void attack(Game& g, Entity& attacker, Entity& defender) {
    if (!attacker.alive || !defender.alive) return;

    int roll  = roll_d(g, 20);
    int total = roll + attacker.attack_bonus;
    // Simple target number: 10 + defender AC / 2-ish
    // (You can replace this with proper B/X math later.)
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
        }
    } else {
        std::cout << "Miss!\n";
    }

    std::cout << "Press Enter to continue...";
    std::string dummy;
    std::getline(std::cin, dummy);
}

// ---------------------------
// Input & movement
// ---------------------------
bool is_blocked(const Game& g, int x, int y) {
    if (!in_bounds(g, x, y)) return true;
    const Tile& t = g.map[idx(g, x, y)];
    if (t.type == TileType::Wall) return true;
    return false;
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

int sign(int v) {
    if (v < 0) return -1;
    if (v > 0) return 1;
    return 0;
}

// Very dumb goblin AI: walk straight toward hero, attack if adjacent.
void goblin_turn(Game& g) {
    if (!g.goblin.alive) return;
    if (!g.hero.alive)   return;

    int dx = g.hero.pos.x - g.goblin.pos.x;
    int dy = g.hero.pos.y - g.goblin.pos.y;
    int dist = std::abs(dx) + std::abs(dy);

    if (dist == 1) {
        // Adjacent -> attack
        attack(g, g.goblin, g.hero);
        return;
    }

    // Try to move closer (Manhattan)
    int step_x = sign(dx);
    int step_y = sign(dy);

    // Try x first
    if (step_x != 0 && !is_blocked(g, g.goblin.pos.x + step_x, g.goblin.pos.y)) {
        move_entity(g, g.goblin, step_x, 0);
    } else if (step_y != 0 && !is_blocked(g, g.goblin.pos.x, g.goblin.pos.y + step_y)) {
        move_entity(g, g.goblin, 0, step_y);
    }
}

bool handle_player_input(Game& g) {
    std::cout << "> ";
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    if (line.empty()) return true;

    char c = line[0];
    int dx = 0, dy = 0;

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

        // If goblin is there, attack
        if (g.goblin.alive && g.goblin.pos.x == nx && g.goblin.pos.y == ny) {
            attack(g, g.hero, g.goblin);
        } else {
            move_entity(g, g.hero, dx, dy);
        }
    }

    return true;
}

// ---------------------------
// Main loop
// ---------------------------
int main() {
    Game g;
    init_game(g);

    bool running = true;
    while (running) {
        render(g);

        if (!g.hero.alive || !g.goblin.alive) {
            std::cout << "\nGame over.\n";
            if (!g.hero.alive && !g.goblin.alive) {
                std::cout << "You died, but took the goblin with you.\n";
            } else if (!g.hero.alive) {
                std::cout << "You have been slain by the goblin.\n";
            } else {
                std::cout << "You have slain the goblin!\n";
            }
            break;
        }

        running = handle_player_input(g);
        if (!running) break;

        if (g.hero.alive && g.goblin.alive) {
            goblin_turn(g);
        }
    }

    std::cout << "Thanks for playing.\n";
    return 0;
}

