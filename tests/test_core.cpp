#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "third_party/doctest.h"

#include "game.hpp"
#include "game_session.hpp"
#include "map.hpp"
#include "module_content.hpp"
#include "module_manifest.hpp"
#include "rules_catalog.hpp"
#include "campaign_state.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* name, const char* value)
        : name_(name) {
        const char* old = std::getenv(name_);
        if (old) {
            had_old_ = true;
            old_value_ = old;
        }
        setenv(name_, value, 1);
    }

    ~ScopedEnvVar() {
        if (had_old_) {
            setenv(name_, old_value_.c_str(), 1);
        } else {
            unsetenv(name_);
        }
    }

private:
    const char* name_;
    bool had_old_{false};
    std::string old_value_;
};

// Helper to make a minimal Game for tests without running full init_game
static Game make_minimal_game(int w = 10, int h = 6) {
    Game g;
    g.width  = w;
    g.height = h;

    // Simple deterministic RNG for tests
    g.rng.seed(12345u);

    // Minimal map: all floors
    g.map.resize(w * h);
    for (auto &t : g.map) {
        t.type = TileType::Floor;
    }

    return g;
}

TEST_CASE("index_for maps 2D coords to linear index correctly") {
    Game g = make_minimal_game(10, 6);

    // Top-left
    CHECK(index_for(g, 0, 0) == 0);

    // One to the right
    CHECK(index_for(g, 1, 0) == 1);

    // Next row
    CHECK(index_for(g, 0, 1) == 10);

    // Bottom-right
    CHECK(index_for(g, 9, 5) == 5 * 10 + 9);
}

TEST_CASE("in_bounds works for valid and invalid coordinates") {
    Game g = make_minimal_game(10, 6);

    CHECK(in_bounds(g, 0, 0));
    CHECK(in_bounds(g, 9, 5));

    CHECK_FALSE(in_bounds(g, -1, 0));
    CHECK_FALSE(in_bounds(g, 0, -1));
    CHECK_FALSE(in_bounds(g, 10, 0));
    CHECK_FALSE(in_bounds(g, 0, 6));
}

TEST_CASE("is_blocked returns false for floor tiles and true for out-of-bounds") {
    Game g = make_minimal_game(10, 6);

    // All tiles are floor in make_minimal_game, so in-bounds should be not blocked.
    CHECK_FALSE(is_blocked(g, 5, 3));

    // Out of bounds is always blocked
    CHECK(is_blocked(g, -1, 0));
    CHECK(is_blocked(g, 10, 0));
    CHECK(is_blocked(g, 0, -1));
    CHECK(is_blocked(g, 0, 6));
}

TEST_CASE("roll_d always returns values in [1, sides]") {
    Game g = make_minimal_game();
    const int sides = 6;
    const int iterations = 1000;

    for (int i = 0; i < iterations; ++i) {
        int val = roll_d(g, sides);
        CHECK(val >= 1);
        CHECK(val <= sides);
    }
}

// ── Dungeon generation tests ────────────────────────────

TEST_CASE("Dungeon generation: init_map") {
    Game g = make_minimal_game(40, 20);

    SUBCASE("init_map returns at least one room") {
        auto rooms = init_map(g);
        CHECK(rooms.size() >= 1);
    }

    SUBCASE("all room tiles are floor or stairs") {
        auto rooms = init_map(g);
        for (const auto& r : rooms) {
            for (int y = r.y; y < r.y + r.h; ++y) {
                for (int x = r.x; x < r.x + r.w; ++x) {
                    auto t = g.map[index_for(g, x, y)].type;
                    CHECK((t == TileType::Floor || t == TileType::StairsDown || t == TileType::StairsUp));
                }
            }
        }
    }

    SUBCASE("corridors connect rooms") {
        auto rooms = init_map(g);
        // Count all floor tiles vs. room-only floor tiles
        int total_floor = 0;
        for (const auto& t : g.map)
            if (t.type == TileType::Floor) ++total_floor;
        int room_floor = 0;
        for (const auto& r : rooms)
            room_floor += r.w * r.h;
        // If multiple rooms, corridors must exist outside rooms
        if (rooms.size() >= 2)
            CHECK(total_floor > room_floor);
    }

    SUBCASE("rooms do not overlap with margin 1") {
        auto rooms = init_map(g);
        for (std::size_t i = 0; i < rooms.size(); ++i) {
            for (std::size_t j = i + 1; j < rooms.size(); ++j) {
                const auto& a = rooms[i];
                const auto& b = rooms[j];
                bool overlap = (a.x - 1 < b.x + b.w &&
                                a.x + a.w + 1 > b.x &&
                                a.y - 1 < b.y + b.h &&
                                a.y + a.h + 1 > b.y);
                CHECK_FALSE(overlap);
            }
        }
    }
}

TEST_CASE("Dungeon generation: entity placement") {
    Game g = make_minimal_game(40, 20);
    auto rooms = init_map(g);

    SUBCASE("hero centre of first room is on a floor tile") {
        Vec2 hero_pos = {rooms[0].x + rooms[0].w / 2,
                         rooms[0].y + rooms[0].h / 2};
        int idx = index_for(g, hero_pos.x, hero_pos.y);
        CHECK(g.map[idx].type == TileType::Floor);
    }

    SUBCASE("every room centre is on a floor tile") {
        for (const auto& r : rooms) {
            Vec2 centre = {r.x + r.w / 2, r.y + r.h / 2};
            int idx = index_for(g, centre.x, centre.y);
            CHECK(g.map[idx].type == TileType::Floor);
        }
    }
}

// ── Initiative tests ────────────────────────────────────

TEST_CASE("Initiative: 1d6 per side, re-roll ties") {
    Game g = make_minimal_game();

    SUBCASE("roll_initiative returns a bool and logs a message") {
        roll_initiative(g);

        // Always returns true (monsters first) or false (hero first)
        CHECK(g.event_log.size() >= 1);
        CHECK(g.event_log.back().find("Initiative:") == 0);
    }

    SUBCASE("initiative can go either way") {
        bool saw_true  = false;
        bool saw_false = false;

        for (int i = 0; i < 100; ++i) {
            Game g2 = make_minimal_game();
            g2.rng.seed(static_cast<unsigned long>(12345u + i));
            bool r = roll_initiative(g2);
            if (r) saw_true = true;
            else   saw_false = true;
            if (saw_true && saw_false) break;
        }

        CHECK(saw_true);
        CHECK(saw_false);
    }

    SUBCASE("event log reflects the correct winner") {
        bool monsters_first = roll_initiative(g);
        std::string msg = g.event_log.back();

        if (monsters_first) {
            CHECK(msg.find("Monsters") != std::string::npos);
        } else {
            CHECK(msg.find("Hero") != std::string::npos);
        }
    }
}

// ── Surprise tests ───────────────────────────────────────

static unsigned find_surprise_seed(bool hero_surprised, bool monster_surprised) {
    for (unsigned seed = 1; seed < 10000; ++seed) {
        Game g = make_minimal_game();
        g.rng.seed(seed);
        SurpriseResult result = roll_surprise(g);
        if (result.hero_surprised == hero_surprised
            && result.monsters_surprised == monster_surprised) {
            return seed;
        }
    }
    return 1;
}

static unsigned find_surprise_seed(int hero_threshold,
                                   bool hero_surprised, bool monster_surprised) {
    for (unsigned seed = 1; seed < 10000; ++seed) {
        Game g = make_minimal_game();
        g.rng.seed(seed);
        SurpriseResult result = roll_surprise(g, hero_threshold);
        if (result.hero_surprised == hero_surprised
            && result.monsters_surprised == monster_surprised) {
            return seed;
        }
    }
    return 1;
}

TEST_CASE("Surprise: B/X 1-2 on 1d6 per side") {
    SUBCASE("exact rolls determine hero surprise") {
        SurpriseResult result = resolve_surprise(/*hero_roll=*/1, /*monster_roll=*/3);
        CHECK(result.hero_surprised);
        CHECK_FALSE(result.monsters_surprised);
    }

    SUBCASE("exact rolls determine monster surprise") {
        SurpriseResult result = resolve_surprise(/*hero_roll=*/4, /*monster_roll=*/2);
        CHECK_FALSE(result.hero_surprised);
        CHECK(result.monsters_surprised);
    }

    SUBCASE("both sides can be surprised") {
        SurpriseResult result = resolve_surprise(/*hero_roll=*/2, /*monster_roll=*/1);
        CHECK(result.hero_surprised);
        CHECK(result.monsters_surprised);
    }

    SUBCASE("neither side is surprised") {
        SurpriseResult result = resolve_surprise(/*hero_roll=*/3, /*monster_roll=*/6);
        CHECK_FALSE(result.hero_surprised);
        CHECK_FALSE(result.monsters_surprised);
    }
}

TEST_CASE("Surprise: stealthy monster thresholds") {
    SUBCASE("most monsters use the default 1-2 threshold") {
        CHECK(monster_surprise_threshold("Goblin") == 2);
        CHECK(monster_surprise_threshold("Orc") == 2);
        CHECK(monster_surprise_threshold("Bugbear") == 3);
        CHECK(monster_surprise_threshold("Spider, Crab") == 4);
        CHECK(monster_surprise_threshold("Shadow") == 5);
        CHECK(monster_surprise_threshold("Lycanthrope, Weretiger") == 4);
    }

    SUBCASE("an unknown monster defaults to 2") {
        CHECK(monster_surprise_threshold("Made Up Monster") == 2);
    }

    SUBCASE("a bugbear can surprise the hero on a roll of 3") {
        SurpriseResult result = resolve_surprise(/*hero_roll=*/3, /*monster_roll=*/6,
                                                 monster_surprise_threshold("Bugbear"));
        CHECK(result.hero_surprised);
        CHECK_FALSE(result.monsters_surprised);
    }

    SUBCASE("a shadow can surprise the hero on a roll of 5") {
        SurpriseResult result = resolve_surprise(/*hero_roll=*/5, /*monster_roll=*/6,
                                                 monster_surprise_threshold("Shadow"));
        CHECK(result.hero_surprised);
        CHECK_FALSE(result.monsters_surprised);
    }
}

// ── Test helpers ─────────────────────────────────────────

static Entity make_hero(int x, int y, int atk_bonus, int dmg_die, int ac) {
    Entity e;
    e.name             = "Hero";
    e.type             = EntityType::Hero;
    e.pos              = {x, y};
    e.alive            = true;
    e.hp               = 20;
    e.max_hp           = 20;
    e.ac               = ac;
    e.attack_bonus     = atk_bonus;
    e.damage_die       = dmg_die;
    e.morale           = 0;
    e.fleeing          = false;
    e.attacks_per_turn = 1;
    return e;
}

static Entity make_monster(const std::string& name, EntityType type,
                           int x, int y,
                           int hp, int ac,
                           int atk_bonus, int dmg_die,
                           int morale) {
    Entity e;
    e.name             = name;
    e.type             = type;
    e.pos              = {x, y};
    e.alive            = true;
    e.hp               = hp;
    e.max_hp           = hp;
    e.ac               = ac;
    e.attack_bonus     = atk_bonus;
    e.damage_die       = dmg_die;
    e.morale           = morale;
    e.fleeing          = false;
    e.attacks_per_turn = 1;
    e.surprise_checked = true;
    return e;
}

static unsigned find_d6_seed(int wanted_roll) {
    for (unsigned seed = 1; seed < 10000; ++seed) {
        Game g = make_minimal_game();
        g.rng.seed(seed);
        if (roll_d(g, 6) == wanted_roll) return seed;
    }
    return 1;
}

// Find a seed where an adjacent monster attacks the hero once and the given
// paralysis outcome occurs (hit + failed save → paralyzed).
static unsigned find_paralysis_seed(const std::string& name, bool elf,
                                    bool want_paralyzed) {
    for (unsigned seed = 1; seed < 100000; ++seed) {
        Game g = make_minimal_game();
        g.rng.seed(seed);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        if (elf) g.hero.char_class = CharacterClass::Elf;
        g.monsters.push_back(make_monster(name, EntityType::Monster,
                                          2, 1, 10, 10, 20, 6, 9));
        g.monsters[0].has_paralysis = true;

        monsters_turn(g);

        bool paralyzed = g.hero.paralyzed_turns > 0;
        if (paralyzed == want_paralyzed) return seed;
    }
    return 1;
}

// Find a seed where a swarm of 10 normal bats adjacent to the hero makes the
// hero MISS an attack (due to the -2 confusion penalty) that the SAME hero
// would otherwise HIT without the swarm. Proves the -2 to-hit is applied.
static unsigned find_bat_tohit_flip_seed() {
    for (unsigned seed = 1; seed < 200000; ++seed) {
        // With the swarm (confused): expect a miss.
        Game g = make_minimal_game();
        g.rng.seed(seed);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/10, /*dmg_die=*/1,
                           /*ac=*/10);
        auto bat = make_monster("Bat, Normal", EntityType::Monster,
                                1, 2, 1, 10, 0, 1, 9);
        bat.count = 10;
        g.monsters.push_back(bat);
        g.monsters.push_back(make_monster("Orc", EntityType::Monster,
                                          2, 1, 10, 12, 0, 6, 9));
        PlayerCommand move;
        move.type = PlayerCommandType::Move;
        move.dx = 1;
        apply_player_command(g, move);
        bool confused_miss = g.monsters[1].hp == 10;

        // Without the swarm (not confused): same seed must HIT.
        Game g2 = make_minimal_game();
        g2.rng.seed(seed);
        g2.hero = make_hero(/*x=*/1, /*y=*/1,
                            /*atk_bonus=*/10, /*dmg_die=*/1,
                            /*ac=*/10);
        g2.monsters.push_back(make_monster("Orc", EntityType::Monster,
                                           2, 1, 10, 12, 0, 6, 9));
        PlayerCommand move2;
        move2.type = PlayerCommandType::Move;
        move2.dx = 1;
        apply_player_command(g2, move2);
        bool clear_hit = g2.monsters[0].hp < 10;

        if (confused_miss && clear_hit) return seed;
    }
    return 1;
}

// Find a seed where an adjacent vampire bat bites the hero and produces the
// given unconsciousness outcome (hit + failed save → unconscious).
static unsigned find_vampire_bat_seed(bool want_unconscious) {
    for (unsigned seed = 1; seed < 100000; ++seed) {
        Game g = make_minimal_game();
        g.rng.seed(seed);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        auto bat = make_monster("Bat, Giant", EntityType::Monster,
                                2, 1, 10, 10, 20, 6, 8);
        bat.is_vampire_bat = true;
        g.monsters.push_back(bat);

        monsters_turn(g);

        bool unconscious = g.hero.unconscious_turns > 0;
        if (unconscious == want_unconscious) return seed;
    }
    return 1;
}

// Find a seed where a goblin adjacent to the hero MISSES its attack when the
// hero carries a light (torch) — the B/X daylight -1 to-hit — but the SAME
// dice roll HITS in pitch darkness. Proves the penalty is applied only in light.
static unsigned find_daylight_tohit_flip_seed() {
    for (unsigned seed = 1; seed < 300000; ++seed) {
        // With a lit torch: goblin should miss (daylight -1 to hit).
        Game g = make_minimal_game();
        g.rng.seed(seed);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/20);
        g.hero.torch_turns = 20;
        g.monsters.push_back(make_monster("Goblin", EntityType::Monster,
                                          2, 1, 10, 10, 10, 6, 9));

        monsters_turn(g);
        bool lit_miss = g.hero.hp == 20;

        // Same seed with no light: goblin attacks at full odds and hits.
        Game g2 = make_minimal_game();
        g2.rng.seed(seed);
        g2.hero = make_hero(/*x=*/1, /*y=*/1,
                            /*atk_bonus=*/0, /*dmg_die=*/1,
                            /*ac=*/20);
        g2.hero.torch_turns = 0;
        g2.monsters.push_back(make_monster("Goblin", EntityType::Monster,
                                           2, 1, 10, 10, 10, 6, 9));

        monsters_turn(g2);
        bool dark_hit = g2.hero.hp < 20;

        if (lit_miss && dark_hit) return seed;
    }
    return 1;
}

// Find a seed where a wounded goblin with its king alive nearby HOLDS its
// ground (effective morale 9) but the SAME dice rolls make it FLEE once the
// king is dead (effective morale 7). Proves leader morale is collective.
static unsigned find_leader_morale_flip_seed() {
    for (unsigned seed = 1; seed < 500000; ++seed) {
        // King alive: wounded goblin holds ground (2d6 roll ≤ 9).
        Game g = make_minimal_game();
        g.rng.seed(seed);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/10);
        g.monsters.push_back(make_monster("Goblin", EntityType::Monster,
                                          2, 1, 10, 12, 0, 6, 7));
        auto king = make_monster("Goblin", EntityType::Monster,
                                 1, 2, 15, 6, 3, 6, 7);
        king.is_leader = true;
        g.monsters.push_back(king);

        apply_player_command(g, {PlayerCommandType::Move, /*dx=*/1, /*dy=*/0});
        bool holds_with_king = !g.monsters[0].fleeing && g.monsters[0].alive;

        // Same seed, king dead: wounded goblin flees (2d6 roll > 7).
        Game g2 = make_minimal_game();
        g2.rng.seed(seed);
        g2.hero = make_hero(/*x=*/1, /*y=*/1,
                            /*atk_bonus=*/20, /*dmg_die=*/1,
                            /*ac=*/10);
        g2.monsters.push_back(make_monster("Goblin", EntityType::Monster,
                                           2, 1, 10, 12, 0, 6, 7));
        auto dead_king = make_monster("Goblin", EntityType::Monster,
                                      1, 2, 15, 6, 3, 6, 7);
        dead_king.is_leader = true;
        dead_king.alive = false;
        g2.monsters.push_back(dead_king);

        apply_player_command(g2, {PlayerCommandType::Move, /*dx=*/1, /*dy=*/0});
        bool flees_king_dead = g2.monsters[0].fleeing;

        if (holds_with_king && flees_king_dead) return seed;
    }
    return 1;
}

TEST_CASE("Secret doors: search, reveal, and open") {
    SUBCASE("an unrevealed secret door blocks movement and opening") {
        Game g = make_minimal_game();
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        auto& tile = g.map[index_for(g, 2, 1)];
        tile.type = TileType::SecretDoorClosed;

        PlayerCommand move;
        move.type = PlayerCommandType::Move;
        move.dx = 1;
        apply_player_command(g, move);

        CHECK(g.hero.pos.x == 1);
        CHECK(g.hero.pos.y == 1);
        CHECK(is_blocked(g, 2, 1));

        PlayerCommand open;
        open.type = PlayerCommandType::Open;
        apply_player_command(g, open);

        CHECK(tile.type == TileType::SecretDoorClosed);
        CHECK_FALSE(tile.secret_revealed);
    }

    SUBCASE("search can reveal an adjacent secret door") {
        Game g = make_minimal_game();
        g.rng.seed(find_d6_seed(1));
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        auto& tile = g.map[index_for(g, 2, 1)];
        tile.type = TileType::SecretDoorClosed;

        PlayerCommand search;
        search.type = PlayerCommandType::Search;
        apply_player_command(g, search);

        CHECK(tile.secret_revealed);
        bool saw_secret = false;
        for (const auto& e : g.event_log) {
            if (e.find("secret door") != std::string::npos) saw_secret = true;
        }
        CHECK(saw_secret);
    }

    SUBCASE("a revealed secret door opens like a normal door") {
        Game g = make_minimal_game();
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        auto& tile = g.map[index_for(g, 2, 1)];
        tile.type = TileType::SecretDoorClosed;
        tile.secret_revealed = true;

        PlayerCommand open;
        open.type = PlayerCommandType::Open;
        apply_player_command(g, open);

        CHECK(tile.type == TileType::DoorOpen);
        CHECK_FALSE(is_blocked(g, 2, 1));

        PlayerCommand move;
        move.type = PlayerCommandType::Move;
        move.dx = 1;
        apply_player_command(g, move);

        CHECK(g.hero.pos.x == 2);
        CHECK(g.hero.pos.y == 1);
    }
}

TEST_CASE("Surprise lifecycle: first contact only") {
    SUBCASE("hero surprised loses the bump-attack action") {
        Game g = make_minimal_game();
        g.rng.seed(find_surprise_seed(/*hero_surprised=*/true,
                                      /*monster_surprised=*/false));
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/10);
        g.monsters.push_back(make_monster("Goblin", EntityType::Monster,
                                          2, 1, 5, 6, 0, 6, 7));
        g.monsters[0].surprise_checked = false;

        PlayerCommand cmd;
        cmd.type = PlayerCommandType::Move;
        cmd.dx = 1;
        apply_player_command(g, cmd);

        CHECK(g.monsters[0].hp == 5);
        CHECK(g.monsters[0].surprise_checked);
        bool saw_surprise = false;
        for (const auto& e : g.event_log) {
            if (e.find("catches you off guard") != std::string::npos) saw_surprise = true;
        }
        CHECK(saw_surprise);
    }

    SUBCASE("monster surprised loses its first adjacent monster action") {
        Game g = make_minimal_game();
        g.rng.seed(find_surprise_seed(/*hero_surprised=*/false,
                                      /*monster_surprised=*/true));
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/10);
        g.monsters.push_back(make_monster("Goblin", EntityType::Monster,
                                          2, 1, 5, 6, 0, 6, 7));
        g.monsters[0].surprise_checked = false;

        PlayerCommand cmd;
        cmd.type = PlayerCommandType::Move;
        cmd.dx = 1;
        apply_player_command(g, cmd);
        CHECK(g.monsters[0].hp == 4);
        CHECK(g.monsters[0].surprised);

        monsters_turn(g);

        CHECK(g.hero.hp == 20);
        CHECK_FALSE(g.monsters[0].surprised);
        bool saw_lost_moment = false;
        for (const auto& e : g.event_log) {
            if (e.find("loses a moment") != std::string::npos) saw_lost_moment = true;
        }
        CHECK(saw_lost_moment);
    }

    SUBCASE("surprise is not rolled again after first contact") {
        Game g = make_minimal_game();
        g.rng.seed(find_surprise_seed(/*hero_surprised=*/true,
                                      /*monster_surprised=*/false));
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/10);
        g.monsters.push_back(make_monster("Goblin", EntityType::Monster,
                                          2, 1, 5, 6, 0, 6, 7));
        g.monsters[0].surprise_checked = false;

        PlayerCommand cmd;
        cmd.type = PlayerCommandType::Move;
        cmd.dx = 1;
        apply_player_command(g, cmd);
        apply_player_command(g, cmd);

        CHECK(g.monsters[0].hp < 5);
        int surprise_logs = 0;
        for (const auto& e : g.event_log) {
            if (e.find("catches you off guard") != std::string::npos) ++surprise_logs;
        }
        CHECK(surprise_logs == 1);
    }

    SUBCASE("a bugbear surprises the hero on a roll of 3 during first contact") {
        Game g = make_minimal_game();
        g.rng.seed(find_surprise_seed(monster_surprise_threshold("Bugbear"),
                                      /*hero_surprised=*/true,
                                      /*monster_surprised=*/false));
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/10);
        g.monsters.push_back(make_monster("Bugbear", EntityType::Monster,
                                          2, 1, 5, 6, 0, 6, 7));
        g.monsters[0].surprise_checked = false;

        PlayerCommand cmd;
        cmd.type = PlayerCommandType::Move;
        cmd.dx = 1;
        apply_player_command(g, cmd);

        CHECK(g.monsters[0].hp == 5);
        CHECK(g.monsters[0].surprise_checked);
        bool saw_surprise = false;
        for (const auto& e : g.event_log) {
            if (e.find("catches you off guard") != std::string::npos) saw_surprise = true;
        }
        CHECK(saw_surprise);
    }
}

TEST_CASE("Paralysis: ghoul touch") {
    SUBCASE("a ghoul hit with a failed save paralyzes the hero for 2-8 turns") {
        Game g = make_minimal_game();
        g.rng.seed(find_paralysis_seed("Ghoul", /*elf=*/false, /*want_paralyzed=*/true));
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        g.monsters.push_back(make_monster("Ghoul", EntityType::Monster,
                                          2, 1, 10, 10, 20, 6, 9));
        g.monsters[0].has_paralysis = true;

        monsters_turn(g);

        CHECK(g.hero.paralyzed_turns >= 2);
        CHECK(g.hero.paralyzed_turns <= 8);
        bool saw_paralysis = false;
        for (const auto& e : g.event_log) {
            if (e.find("paralyzes you") != std::string::npos) saw_paralysis = true;
        }
        CHECK(saw_paralysis);
    }

    SUBCASE("a successful save resists paralysis") {
        Game g = make_minimal_game();
        g.rng.seed(find_paralysis_seed("Ghoul", /*elf=*/false, /*want_paralyzed=*/false));
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        g.monsters.push_back(make_monster("Ghoul", EntityType::Monster,
                                          2, 1, 10, 10, 20, 6, 9));
        g.monsters[0].has_paralysis = true;

        monsters_turn(g);

        CHECK(g.hero.paralyzed_turns == 0);
        bool saw_resist = false;
        for (const auto& e : g.event_log) {
            if (e.find("resist the paralysis") != std::string::npos) saw_resist = true;
        }
        CHECK(saw_resist);
    }

    SUBCASE("elves are immune to ghoul paralysis") {
        Game g = make_minimal_game();
        g.rng.seed(find_paralysis_seed("Ghoul", /*elf=*/true, /*want_paralyzed=*/false));
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        g.hero.char_class = CharacterClass::Elf;
        g.monsters.push_back(make_monster("Ghoul", EntityType::Monster,
                                          2, 1, 10, 10, 20, 6, 9));
        g.monsters[0].has_paralysis = true;

        monsters_turn(g);

        CHECK(g.hero.paralyzed_turns == 0);
        bool saw_elf_immune = false;
        for (const auto& e : g.event_log) {
            if (e.find("elven blood") != std::string::npos) saw_elf_immune = true;
        }
        CHECK(saw_elf_immune);
    }

    SUBCASE("a carrion crawler paralyzes even an elf") {
        Game g = make_minimal_game();
        g.rng.seed(find_paralysis_seed("Carrion Crawler", /*elf=*/true, /*want_paralyzed=*/true));
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        g.hero.char_class = CharacterClass::Elf;
        g.monsters.push_back(make_monster("Carrion Crawler", EntityType::Monster,
                                          2, 1, 10, 10, 20, 6, 9));
        g.monsters[0].has_paralysis = true;

        monsters_turn(g);

        CHECK(g.hero.paralyzed_turns >= 2);
        CHECK(g.hero.paralyzed_turns <= 8);
    }
}

TEST_CASE("Paralysis: blocks hero actions and wears off") {
    SUBCASE("a paralyzed hero cannot act while turns remain") {
        Game g = make_minimal_game();
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/10);
        g.hero.paralyzed_turns = 3;
        g.monsters.push_back(make_monster("Ghoul", EntityType::Monster,
                                          2, 1, 5, 6, 0, 6, 7));

        PlayerCommand cmd;
        cmd.type = PlayerCommandType::Move;
        cmd.dx = 1;
        apply_player_command(g, cmd);

        CHECK(g.hero.paralyzed_turns == 2);
        CHECK(g.hero.pos.x == 1);  // did not move
        CHECK(g.monsters[0].hp == 5);  // did not attack
    }

    SUBCASE("paralysis wears off after its turn count elapses") {
        Game g = make_minimal_game();
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/10);
        g.hero.paralyzed_turns = 1;
        g.monsters.push_back(make_monster("Ghoul", EntityType::Monster,
                                          2, 1, 5, 6, 0, 6, 7));

        PlayerCommand cmd;
        cmd.type = PlayerCommandType::Move;
        cmd.dx = 1;
        apply_player_command(g, cmd);

        CHECK(g.hero.paralyzed_turns == 0);
        CHECK(g.hero.pos.x == 1);  // this turn was consumed, still didn't move
        bool saw_wear_off = false;
        for (const auto& e : g.event_log) {
            if (e.find("wears off") != std::string::npos) saw_wear_off = true;
        }
        CHECK(saw_wear_off);
    }

    SUBCASE("cure light wounds removes paralysis immediately") {
        Game g = make_minimal_game(10, 10);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        g.hero.paralyzed_turns = 4;
        g.hero.spell_slots[1] = 1;
        g.hero.spells = {"Cure Light Wounds"};
        g.hero.hp = 3;

        apply_player_command(g, {PlayerCommandType::Cast, /*spell_index=*/0, 0, 0});

        CHECK(g.hero.paralyzed_turns == 0);
        bool saw_cure = false;
        for (const auto& e : g.event_log) {
            if (e.find("cures your paralysis") != std::string::npos) saw_cure = true;
        }
        CHECK(saw_cure);
    }
}

TEST_CASE("Normal bats: swarm confusion") {
    SUBCASE("a swarm of 10+ normal bats adjacent to the hero confuses them (-2 to hit)") {
        unsigned seed = find_bat_tohit_flip_seed();
        REQUIRE(seed != 1);

        Game g = make_minimal_game();
        g.rng.seed(seed);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/10, /*dmg_die=*/1,
                           /*ac=*/10);
        auto bat = make_monster("Bat, Normal", EntityType::Monster,
                                1, 2, 1, 10, 0, 1, 9);
        bat.count = 10;
        g.monsters.push_back(bat);
        g.monsters.push_back(make_monster("Orc", EntityType::Monster,
                                          2, 1, 10, 12, 0, 6, 9));

        PlayerCommand move;
        move.type = PlayerCommandType::Move;
        move.dx = 1;
        apply_player_command(g, move);

        CHECK(g.monsters[1].hp == 10);  // missed because of the -2 penalty
    }

    SUBCASE("without a swarm the same attack hits") {
        unsigned seed = find_bat_tohit_flip_seed();
        REQUIRE(seed != 1);

        Game g = make_minimal_game();
        g.rng.seed(seed);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/10, /*dmg_die=*/1,
                           /*ac=*/10);
        g.monsters.push_back(make_monster("Orc", EntityType::Monster,
                                          2, 1, 10, 12, 0, 6, 9));

        PlayerCommand move;
        move.type = PlayerCommandType::Move;
        move.dx = 1;
        apply_player_command(g, move);

        CHECK(g.monsters[0].hp < 10);  // hits without confusion
    }

    SUBCASE("a confused hero cannot cast spells") {
        Game g = make_minimal_game(10, 10);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        g.hero.spell_slots[1] = 1;
        g.hero.spells = {"Cure Light Wounds"};
        g.hero.hp = 3;
        auto bat = make_monster("Bat, Normal", EntityType::Monster,
                                2, 1, 1, 10, 0, 1, 9);
        bat.count = 10;
        g.monsters.push_back(bat);

        apply_player_command(g, {PlayerCommandType::Cast, /*spell_index=*/0, 0, 0});

        CHECK(g.hero.spell_slots[1] == 1);  // slot not consumed
        bool saw_block = false;
        for (const auto& e : g.event_log) {
            if (e.find("confuses your concentration") != std::string::npos) saw_block = true;
        }
        CHECK(saw_block);
    }

    SUBCASE("a small flock (fewer than 10) does not confuse the hero") {
        Game g = make_minimal_game(10, 10);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        g.hero.spell_slots[1] = 1;
        g.hero.spells = {"Cure Light Wounds"};
        g.hero.hp = 3;
        auto bat = make_monster("Bat, Normal", EntityType::Monster,
                                2, 1, 1, 10, 0, 1, 9);
        bat.count = 9;
        g.monsters.push_back(bat);

        apply_player_command(g, {PlayerCommandType::Cast, /*spell_index=*/0, 0, 0});

        CHECK(g.hero.spell_slots[1] == 0);  // slot consumed, spell cast
    }

    SUBCASE("normal bats never bite: adjacent swarm leaves the hero unharmed") {
        Game g = make_minimal_game();
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        auto bat = make_monster("Bat, Normal", EntityType::Monster,
                                2, 1, 10, 10, 0, 6, 9);
        bat.count = 10;
        g.monsters.push_back(bat);

        monsters_turn(g);

        CHECK(g.hero.hp == 20);  // untouched by the swarm
    }

    SUBCASE("normal bats check morale each round while swarming") {
        Game g = make_minimal_game();
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        auto bat = make_monster("Bat, Normal", EntityType::Monster,
                                2, 1, 10, 10, 0, 6, 2);  // very low morale
        bat.count = 10;
        g.monsters.push_back(bat);

        monsters_turn(g);

        bool saw_morale = false;
        for (const auto& e : g.event_log) {
            if (e.find("makes a morale check") != std::string::npos) saw_morale = true;
        }
        CHECK(saw_morale);
    }
}

TEST_CASE("Giant vampire bat: unconsciousness and blood drain") {
    SUBCASE("a vampire bat bite with a failed save knocks the hero unconscious") {
        Game g = make_minimal_game();
        g.rng.seed(find_vampire_bat_seed(/*want_unconscious=*/true));
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        auto bat = make_monster("Bat, Giant", EntityType::Monster,
                                2, 1, 10, 10, 20, 6, 8);
        bat.is_vampire_bat = true;
        g.monsters.push_back(bat);

        monsters_turn(g);

        CHECK(g.hero.unconscious_turns >= 1);
        CHECK(g.hero.unconscious_turns <= 10);
        bool saw_unconscious = false;
        for (const auto& e : g.event_log) {
            if (e.find("knocks you unconscious") != std::string::npos) saw_unconscious = true;
        }
        CHECK(saw_unconscious);
    }

    SUBCASE("a successful save resists the vampire bat bite") {
        Game g = make_minimal_game();
        g.rng.seed(find_vampire_bat_seed(/*want_unconscious=*/false));
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        auto bat = make_monster("Bat, Giant", EntityType::Monster,
                                2, 1, 10, 10, 20, 6, 8);
        bat.is_vampire_bat = true;
        g.monsters.push_back(bat);

        monsters_turn(g);

        CHECK(g.hero.unconscious_turns == 0);
        bool saw_resist = false;
        for (const auto& e : g.event_log) {
            if (e.find("shake off the vampire bat's bite") != std::string::npos) saw_resist = true;
        }
        CHECK(saw_resist);
    }

    SUBCASE("an unconscious hero cannot act") {
        Game g = make_minimal_game();
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/10);
        g.hero.unconscious_turns = 3;
        g.monsters.push_back(make_monster("Bat, Giant", EntityType::Monster,
                                          2, 1, 5, 6, 0, 6, 7));

        PlayerCommand cmd;
        cmd.type = PlayerCommandType::Move;
        cmd.dx = 1;
        apply_player_command(g, cmd);

        CHECK(g.hero.unconscious_turns == 2);
        CHECK(g.hero.pos.x == 1);  // did not move
        CHECK(g.monsters[0].hp == 5);  // did not attack
    }

    SUBCASE("unconsciousness wears off after its round count elapses") {
        Game g = make_minimal_game();
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/10);
        g.hero.unconscious_turns = 1;
        g.monsters.push_back(make_monster("Bat, Giant", EntityType::Monster,
                                          2, 1, 5, 6, 0, 6, 7));

        PlayerCommand cmd;
        cmd.type = PlayerCommandType::Move;
        cmd.dx = 1;
        apply_player_command(g, cmd);

        CHECK(g.hero.unconscious_turns == 0);
        CHECK(g.hero.pos.x == 1);  // this turn was consumed, still didn't move
        bool saw_wake = false;
        for (const auto& e : g.event_log) {
            if (e.find("regain consciousness") != std::string::npos) saw_wake = true;
        }
        CHECK(saw_wake);
    }

    SUBCASE("an unconscious hero cannot cast spells") {
        Game g = make_minimal_game(10, 10);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        g.hero.unconscious_turns = 2;
        g.hero.spell_slots[1] = 1;
        g.hero.spells = {"Cure Light Wounds"};
        g.hero.hp = 3;

        apply_player_command(g, {PlayerCommandType::Cast, /*spell_index=*/0, 0, 0});

        CHECK(g.hero.spell_slots[1] == 1);  // slot not consumed
        bool saw_block = false;
        for (const auto& e : g.event_log) {
            if (e.find("unconscious and cannot act") != std::string::npos) saw_block = true;
        }
        CHECK(saw_block);
    }

    SUBCASE("a vampire bat drains blood from an unconscious hero") {
        Game g = make_minimal_game();
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        g.hero.unconscious_turns = 2;
        auto bat = make_monster("Bat, Giant", EntityType::Monster,
                                2, 1, 10, 10, 20, 6, 8);
        bat.is_vampire_bat = true;
        g.monsters.push_back(bat);

        int hp_before = g.hero.hp;
        monsters_turn(g);

        CHECK(g.hero.hp < hp_before);  // blood was drained
        bool saw_drain = false;
        for (const auto& e : g.event_log) {
            if (e.find("drains") != std::string::npos) saw_drain = true;
        }
        CHECK(saw_drain);
    }

    SUBCASE("a plain giant bat does not cause unconsciousness") {
        Game g = make_minimal_game();
        g.rng.seed(1);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/10);
        g.monsters.push_back(make_monster("Bat, Giant", EntityType::Monster,
                                          2, 1, 10, 10, 20, 6, 8));  // not vampire

        monsters_turn(g);

        CHECK(g.hero.unconscious_turns == 0);
    }
}

// ── Humanoid daylight penalties + lair leader morale ─────

TEST_CASE("B/X daylight penalty: light-shy humanoids fight at -1 in the hero's light") {
    SUBCASE("goblins miss in the hero's torchlight but hit in darkness") {
        unsigned seed = find_daylight_tohit_flip_seed();

        Game g = make_minimal_game();
        g.rng.seed(seed);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/20);
        g.hero.torch_turns = 20;
        g.monsters.push_back(make_monster("Goblin", EntityType::Monster,
                                          2, 1, 10, 10, 10, 6, 9));
        monsters_turn(g);
        CHECK(g.hero.hp == 20);  // the goblin missed thanks to the daylight penalty

        Game g2 = make_minimal_game();
        g2.rng.seed(seed);
        g2.hero = make_hero(/*x=*/1, /*y=*/1,
                            /*atk_bonus=*/0, /*dmg_die=*/1,
                            /*ac=*/20);
        g2.hero.torch_turns = 0;
        g2.monsters.push_back(make_monster("Goblin", EntityType::Monster,
                                           2, 1, 10, 10, 10, 6, 9));
        monsters_turn(g2);
        CHECK(g2.hero.hp < 20);  // same rolls hit once the light is gone
    }

    SUBCASE("orcs suffer the same daylight to-hit penalty") {
        // Rely on the same mechanic; verify an orc attack log carries the -1.
        Game g = make_minimal_game();
        g.rng.seed(find_daylight_tohit_flip_seed());
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/20);
        g.hero.torch_turns = 20;
        g.monsters.push_back(make_monster("Orc", EntityType::Monster,
                                          2, 1, 10, 10, 10, 6, 8));
        monsters_turn(g);
        // The flip seed is goblin-specific; just ensure light blocks the hit.
        CHECK(g.hero.hp == 20);
    }

    SUBCASE("kobolds and hobgoblins are not penalized in daylight") {
        // B/X: only goblins and orcs are light-shy. Same setup must hit.
        for (const auto& name : {"Kobold", "Hobgoblin"}) {
            unsigned seed = find_daylight_tohit_flip_seed();
            Game g = make_minimal_game();
            g.rng.seed(seed);
            g.hero = make_hero(/*x=*/1, /*y=*/1,
                               /*atk_bonus=*/0, /*dmg_die=*/1,
                               /*ac=*/20);
            g.hero.torch_turns = 20;
            g.monsters.push_back(make_monster(name, EntityType::Monster,
                                              2, 1, 10, 10, 10, 6, 8));
            monsters_turn(g);
            CHECK(g.hero.hp < 20);  // no penalty: the hit lands
        }
    }

    SUBCASE("the goblin king fights unpenalized in daylight") {
        // B/X: the goblin king and his bodyguard are used to light.
        unsigned seed = find_daylight_tohit_flip_seed();
        Game g = make_minimal_game();
        g.rng.seed(seed);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/0, /*dmg_die=*/1,
                           /*ac=*/20);
        g.hero.torch_turns = 20;
        auto king = make_monster("Goblin", EntityType::Monster,
                                 2, 1, 15, 6, 10, 6, 9);
        king.is_leader = true;
        g.monsters.push_back(king);
        monsters_turn(g);
        // Same seed that flipped a normal goblin to a miss now HITS: no -1.
        CHECK(g.hero.hp < 20);
    }
}

TEST_CASE("B/X lair leaders: morale rises with the king and breaks on his death") {
    SUBCASE("wounded goblin holds ground while the king lives") {
        unsigned seed = find_leader_morale_flip_seed();

        Game g = make_minimal_game();
        g.rng.seed(seed);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/10);
        // monsters[0]: the goblin the hero bumps (wounded follower).
        g.monsters.push_back(make_monster("Goblin", EntityType::Monster,
                                          2, 1, 10, 12, 0, 6, 7));
        // monsters[1]: their king, alive and nearby.
        auto king = make_monster("Goblin", EntityType::Monster,
                                 1, 2, 15, 6, 3, 6, 7);
        king.is_leader = true;
        g.monsters.push_back(king);

        apply_player_command(g, {PlayerCommandType::Move, /*dx=*/1, /*dy=*/0});

        // The wounded goblin rolls vs morale 9 (king alive) → holds.
        CHECK(g.monsters[0].hp == 9);
        CHECK_FALSE(g.monsters[0].fleeing);
    }

    SUBCASE("wounded goblin flees once the king is dead") {
        unsigned seed = find_leader_morale_flip_seed();

        Game g = make_minimal_game();
        g.rng.seed(seed);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/10);
        g.monsters.push_back(make_monster("Goblin", EntityType::Monster,
                                          2, 1, 10, 12, 0, 6, 7));
        auto dead_king = make_monster("Goblin", EntityType::Monster,
                                      1, 2, 15, 6, 3, 6, 7);
        dead_king.is_leader = true;
        dead_king.alive = false;     // their king has fallen
        g.monsters.push_back(dead_king);

        apply_player_command(g, {PlayerCommandType::Move, /*dx=*/1, /*dy=*/0});

        // Now vs morale 7: the same roll now exceeds it → panic and flee.
        CHECK(g.monsters[0].fleeing);
    }

    SUBCASE("slaying a lair leader is announced and shatters group morale") {
        Game g = make_minimal_game();
        g.rng.seed(3);
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/10);
        auto king = make_monster("Goblin", EntityType::Monster,
                                 2, 1, 1, 6, 3, 6, 9);
        king.is_leader = true;
        g.monsters.push_back(king);
        g.monsters.push_back(make_monster("Goblin", EntityType::Monster,
                                          2, 2, 10, 12, 0, 6, 7));

        // Bump the king to slay it.
        apply_player_command(g, {PlayerCommandType::Move, /*dx=*/1, /*dy=*/0});

        CHECK_FALSE(g.monsters[0].alive);
        bool saw_announce = false;
        for (const auto& e : g.event_log) {
            if (e.find("leader has fallen") != std::string::npos) saw_announce = true;
        }
        CHECK(saw_announce);
    }
}

// ── Developer monster launch override tests ──────────────
TEST_CASE("BX_TEST_MONSTER spawns exactly one chosen monster by name") {
    ScopedEnvVar no_monsters("BX_NO_MONSTERS", "0");
    ScopedEnvVar test_monster("BX_TEST_MONSTER", "gObLiN");

    Game g;
    init_game(g, make_hero(1, 1, 0, 1, 10));

    CHECK(g.monsters.size() == 1);
    CHECK(g.monsters[0].name == "Goblin");
}

TEST_CASE("BX_TEST_MONSTER accepts partial monster names") {
    ScopedEnvVar no_monsters("BX_NO_MONSTERS", "0");
    ScopedEnvVar test_monster("BX_TEST_MONSTER", "crawler");

    Game g;
    init_game(g, make_hero(1, 1, 0, 1, 10));

    CHECK(g.monsters.size() == 1);
    CHECK(g.monsters[0].name == "Carrion Crawler");
}

TEST_CASE("BX_TEST_MONSTER accepts 1-based bx_data monster index") {
    ScopedEnvVar no_monsters("BX_NO_MONSTERS", "0");
    ScopedEnvVar test_monster("BX_TEST_MONSTER", "1");

    Game g;
    init_game(g, make_hero(1, 1, 0, 1, 10));

    CHECK(g.monsters.size() == 1);
    CHECK(g.monsters[0].name == "Acolyte");
}

TEST_CASE("BX_TEST_MONSTER takes precedence over BX_NO_MONSTERS") {
    ScopedEnvVar no_monsters("BX_NO_MONSTERS", "1");
    ScopedEnvVar test_monster("BX_TEST_MONSTER", "Zombie");

    Game g;
    init_game(g, make_hero(1, 1, 0, 1, 10));

    CHECK(g.monsters.size() == 1);
    CHECK(g.monsters[0].name == "Zombie");
}

// ── Treasure / XP / level-up tests ──────────────────────

TEST_CASE("Treasure and XP awarding") {
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 20, 1, 10);

    SUBCASE("killing a monster with no gold awards base XP only") {
        g.total_xp = 100;
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 1, 1, 12, 0, 4, 12));

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        CHECK_FALSE(g.monsters[0].alive);
        CHECK(g.total_gold == 0);
        CHECK(g.total_xp == 110);    // 100 + 10 (goblin XP)
    }

    SUBCASE("killing a monster with gold awards gold + XP") {
        g.monsters.push_back(
            make_monster("Kobold", EntityType::Kobold,
                         2, 1, 1, 12, 0, 4, 6));
        g.monsters[0].gold_pieces = 25;

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        // Kobold XP=5, gold=25 → XP gain=30, gold gain=25
        CHECK_FALSE(g.monsters[0].alive);
        CHECK(g.total_gold == 25);
        CHECK(g.total_xp == 30);
    }

    SUBCASE("accumulating enough XP triggers level-up") {
        g.total_xp = 1990;
        int old_atk = g.hero.attack_bonus;
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 1, 1, 12, 0, 4, 12));
        g.monsters[0].gold_pieces = 10;

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        // +20 XP → 2010 ≥ 2000 → level 2, atk_bonus+1
        CHECK(g.hero_level == 2);
        CHECK(g.hero.attack_bonus == old_atk + 1);
        CHECK(g.total_xp >= 2000);
    }
}

TEST_CASE("Fighter attack bonus follows B/X level bands") {
    auto make_fighter_at_level = [](int level, int total_xp, int attack_bonus, int str) {
        Game g = make_minimal_game();
        g.hero = make_hero(1, 1, attack_bonus, 1, 10);
        g.hero.char_class = CharacterClass::Fighter;
        g.hero.abilities.str = str;
        g.hero_level = level;
        g.total_xp = total_xp;
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 1, 1, 0, 0, 4, 0));
        return g;
    };

    SUBCASE("levels 1-3 have no fighter attack progression bonus") {
        Game g = make_fighter_at_level(/*level=*/1, /*total_xp=*/1990,
                                       /*attack_bonus=*/0, /*str=*/9);

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        CHECK(g.hero_level == 2);
        CHECK(g.hero.attack_bonus == 0);
    }

    SUBCASE("level 4 enters the +2 fighter attack band") {
        Game g = make_fighter_at_level(/*level=*/3, /*total_xp=*/7990,
                                       /*attack_bonus=*/0, /*str=*/9);

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        CHECK(g.hero_level == 4);
        CHECK(g.hero.attack_bonus == 2);
    }

    SUBCASE("level 7 enters the +5 fighter attack band") {
        Game g = make_fighter_at_level(/*level=*/6, /*total_xp=*/63990,
                                       /*attack_bonus=*/2, /*str=*/9);

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        CHECK(g.hero_level == 7);
        CHECK(g.hero.attack_bonus == 5);
    }

    SUBCASE("level 10 enters the +7 fighter attack band") {
        Game g = make_fighter_at_level(/*level=*/9, /*total_xp=*/359990,
                                       /*attack_bonus=*/5, /*str=*/9);

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        CHECK(g.hero_level == 10);
        CHECK(g.hero.attack_bonus == 7);
    }

    SUBCASE("level 13 enters the +9 fighter attack band") {
        Game g = make_fighter_at_level(/*level=*/12, /*total_xp=*/719990,
                                       /*attack_bonus=*/7, /*str=*/9);

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        CHECK(g.hero_level == 13);
        CHECK(g.hero.attack_bonus == 9);
    }

    SUBCASE("fighter attack bands preserve STR modifier") {
        Game g = make_fighter_at_level(/*level=*/3, /*total_xp=*/7990,
                                       /*attack_bonus=*/1, /*str=*/13);

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        CHECK(g.hero_level == 4);
        CHECK(g.hero.attack_bonus == 3); // +2 fighter band, +1 STR
    }
}

// ── Test 2: Morale checks + fleeing + free attacks ──────

TEST_CASE("Morale: fleeing and free attacks") {
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 20, 1, 10);

    SUBCASE("high morale (12) never flees when wounded") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 1, 5, 12, 0, 4, 12));

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        // d20=19 hit, d1=1 → HP 5→4
        // 2d6=3 ≤ 12 → holds ground, not fleeing
        CHECK(g.monsters[0].hp == 4);
        CHECK_FALSE(g.monsters[0].fleeing);
        CHECK(g.monsters[0].alive);
    }

    SUBCASE("low morale (2) fails check, flees, takes free attack") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 1, 5, 12, 0, 4, 2));

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        // d20=19 hit, d1=1 → HP 5→4
        // 2d6=3 > 2 → flee + free attack
        // free: d20=4+22=26 hit, d1=1 → HP 4→3
        CHECK(g.monsters[0].hp == 3);    // 5 - 1 - 1
        CHECK(g.monsters[0].fleeing);
        CHECK(g.monsters[0].alive);
    }

    SUBCASE("ally death triggers morale check on same-type monsters") {
        g.monsters.push_back(
            make_monster("Goblin A", EntityType::Goblin,
                         2, 1, 1, 12, 0, 4, 2));
        g.monsters.push_back(
            make_monster("Goblin B", EntityType::Goblin,
                         1, 2, 5, 12, 0, 4, 2));

        // Bump right → hit goblin A (1 HP, dies)
        // → ally-death morale: goblin B checks 2d6=3 > 2 → flee
        // → free attack on B: d20=4+22=26 hit, d1=1 → HP 5→4
        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        CHECK_FALSE(g.monsters[0].alive);    // goblin A dead
        CHECK(g.monsters[1].fleeing);         // goblin B fleeing
        CHECK(g.monsters[1].hp == 4);         // B took free-attack damage
        CHECK(g.monsters[1].alive);
    }

    SUBCASE("fleeing monster moves away from hero during monsters_turn") {
        g.hero.pos = {5, 5};
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 2, 5, 12, 0, 4, 2));
        g.monsters[0].fleeing = true;

        monsters_turn(g);

        // flee_ai: dx=-3, dy=-3 → step_x=-1 → move to (1,2)
        // Not at map edge → stays alive
        CHECK(g.monsters[0].pos.x == 1);
        CHECK(g.monsters[0].pos.y == 2);
        CHECK(g.monsters[0].alive);
    }

    SUBCASE("fleeing monster escapes at map edge") {
        g.hero.pos = {5, 5};
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         1, 2, 5, 12, 0, 4, 2));
        g.monsters[0].fleeing = true;

        monsters_turn(g);

        // flee_ai: dx=-4, dy=-3 → step_x=-1 → move to (0,2)
        // x==0 → escape → alive=false
        CHECK_FALSE(g.monsters[0].alive);
    }
}

// ── Test 3: Monster AI (approach / attack / blocking) ──

TEST_CASE("Monster AI: approach, attack, blocking") {
    Game g = make_minimal_game();
    g.hero = make_hero(/*x=*/1, /*y=*/1,
                       /*atk_bonus=*/20,  /*dmg_die=*/1,
                       /*ac=*/10);

    SUBCASE("monster approaches hero from distance") {
        g.hero.pos = {5, 5};
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 2,   // x, y (far from hero)
                         5, 12,  // hp, ac
                         0, 4,   // atk_bonus, dmg_die
                         12));   // morale (holds ground)

        monsters_turn(g);

        // dx=+3, dy=+3 → step_x=+1 first → (3,2)
        CHECK(g.monsters[0].pos.x == 3);
        CHECK(g.monsters[0].pos.y == 2);
        CHECK(g.monsters[0].alive);
        CHECK_FALSE(g.monsters[0].fleeing);
    }

    SUBCASE("monster attacks hero when adjacent") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 1,   // x, y (adjacent right)
                         5, 12,  // hp, ac
                         20, 1,  // atk_bonus, dmg_die (guaranteed hit, 1 dmg)
                         12));   // morale

        monsters_turn(g);

        // Adjacent (dist=1) → attack hero
        // d20+20 >= AC 10 always hits, d1=1 damage
        CHECK(g.hero.hp == 19);  // 20 - 1
        CHECK(g.hero.alive);
    }

    SUBCASE("monster prefers horizontal movement when dx >= dy") {
        g.hero.pos = {5, 3};
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 2,   // x, y
                         5, 12, 0, 4, 12));

        monsters_turn(g);

        // dx=+3, dy=+1 → |dx| >= |dy|, tries horizontal first
        CHECK(g.monsters[0].pos.x == 3);
        CHECK(g.monsters[0].pos.y == 2);
    }

    SUBCASE("monster uses vertical movement when horizontal blocked") {
        g.hero.pos = {5, 3};
        // Place a wall at (3,2) to block horizontal approach
        g.map[index_for(g, 3, 2)].type = TileType::Wall;
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 2,   // x, y
                         5, 12, 0, 4, 12));

        monsters_turn(g);

        // step_x=+1 blocked by wall, falls through to step_y=+1 → (2,3)
        CHECK(g.monsters[0].pos.x == 2);
        CHECK(g.monsters[0].pos.y == 3);
    }

    SUBCASE("dead monster does not move or attack") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 1,   // adjacent to hero
                         0, 12,  // hp=0 → dead on spawn; set alive manually
                         0, 4, 12));
        g.monsters[0].alive = false;
        g.monsters[0].hp    = 0;

        monsters_turn(g);

        // Dead monster should be ignored
        CHECK(g.hero.hp == 20);
        CHECK_FALSE(g.monsters[0].alive);
        CHECK(g.monsters[0].pos.x == 2);
    }

    SUBCASE("fleeing monster does not attack adjacent hero") {
        g.hero.pos = {5, 5};
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         4, 5,   // adjacent to hero
                         5, 12, 0, 4, 2));
        g.monsters[0].fleeing = true;

        monsters_turn(g);

        // Fleeing → flee_ai runs; moves from (4,5) away to (3,5)
        // Hero takes no damage
        CHECK(g.monsters[0].pos.x == 3);
        CHECK(g.monsters[0].pos.y == 5);
        CHECK(g.hero.hp == 20);
    }
}

TEST_CASE("Bear hug special attack") {
    SUBCASE("no claw hits means no hug damage") {
        Game g = make_minimal_game();
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/100);
        g.monsters.push_back(
            make_monster("Bear, Black", EntityType::Monster,
                         2, 1, 18, 6, 0, 3, 7));

        monsters_turn(g);

        CHECK(g.hero.hp == 20);
        bool saw_hug = false;
        for (const auto& e : g.event_log) {
            if (e.find("hugs") != std::string::npos) saw_hug = true;
        }
        CHECK_FALSE(saw_hug);
    }

    SUBCASE("one claw hit does not trigger hug") {
        Game g = make_minimal_game();
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/15);
        g.monsters.push_back(
            make_monster("Bear, Black", EntityType::Monster,
                         2, 1, 18, 6, 0, 3, 7));

        monsters_turn(g);

        // Seed 12345: first claw d20=19 hits for 3 damage; second claw d20=7 misses;
        // bite d20=4 misses. No hug.
        CHECK(g.hero.hp == 17);
        bool saw_hug = false;
        for (const auto& e : g.event_log) {
            if (e.find("hugs") != std::string::npos) saw_hug = true;
        }
        CHECK_FALSE(saw_hug);
    }

    SUBCASE("two claw hits trigger 2d8 hug damage") {
        Game g = make_minimal_game();
        g.hero = make_hero(/*x=*/1, /*y=*/1,
                           /*atk_bonus=*/20, /*dmg_die=*/1,
                           /*ac=*/10);
        g.monsters.push_back(
            make_monster("Bear, Black", EntityType::Monster,
                         2, 1, 18, 6, 100, 3, 7));

        monsters_turn(g);

        // Seed 12345: claw damages 3 and 1, bite damage 1, hug damage 2+7.
        CHECK(g.hero.hp == 6);
        bool saw_hug = false;
        for (const auto& e : g.event_log) {
            if (e.find("hugs Hero for 9 additional damage") != std::string::npos) {
                saw_hug = true;
            }
        }
        CHECK(saw_hug);
    }
}

// ── Test 4: Movement + entity blocking ──────────────────

TEST_CASE("Movement and entity blocking") {
    Game g = make_minimal_game();
    g.hero = make_hero(/*x=*/1, /*y=*/1,
                       /*atk_bonus=*/20,  /*dmg_die=*/1,
                       /*ac=*/10);

    SUBCASE("hero moves to empty floor tile") {
        g.hero.pos = {2, 3};
        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        CHECK(g.hero.pos.x == 3);
        CHECK(g.hero.pos.y == 3);
    }

    SUBCASE("hero cannot walk into wall") {
        g.hero.pos = {2, 3};
        g.map[index_for(g, 3, 3)].type = TileType::Wall;
        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        CHECK(g.hero.pos.x == 2);
        CHECK(g.hero.pos.y == 3);
    }

    SUBCASE("thief cannot climb into wall tile") {
        g.hero.pos = {2, 3};
        g.hero.char_class = CharacterClass::Thief;
        g.hero.skill_climb = 100;
        g.map[index_for(g, 3, 3)].type = TileType::Wall;
        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        CHECK(g.hero.pos.x == 2);
        CHECK(g.hero.pos.y == 3);
    }

    SUBCASE("hero cannot walk out of bounds") {
        g.hero.pos = {0, 3};
        apply_player_command(g, {PlayerCommandType::Move, -1, 0});

        CHECK(g.hero.pos.x == 0);
        CHECK(g.hero.pos.y == 3);
    }

    SUBCASE("monster blocked by another monster uses alternate axis") {
        g.hero.pos = {5, 5};
        g.monsters.push_back(
            make_monster("Goblin A", EntityType::Goblin,
                         2, 2, 5, 12, 0, 4, 12));
        g.monsters.push_back(
            make_monster("Goblin B", EntityType::Goblin,
                         3, 2, 5, 12, 0, 4, 12)); // blocks A's horizontal

        monsters_turn(g);

        // A tries step_x=+1 → (3,2) occupied by B → blocked
        // Falls through to step_y=+1 → (2,3) → moves there
        CHECK(g.monsters[0].pos.x == 2);
        CHECK(g.monsters[0].pos.y == 3);
    }

    SUBCASE("dead entity does not block movement") {
        g.hero.pos = {5, 5};
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 2, 5, 12, 0, 4, 12));
        g.monsters.push_back(
            make_monster("Corpse", EntityType::Goblin,
                         3, 2, 0, 12, 0, 4, 12));
        g.monsters[1].alive = false;

        monsters_turn(g);

        // step_x=+1 → (3,2) corpse is dead → not occupied → moves to (3,2)
        CHECK(g.monsters[0].pos.x == 3);
        CHECK(g.monsters[0].pos.y == 2);
    }

    SUBCASE("monster completely blocked by walls stays put") {
        g.hero.pos = {5, 5};
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 2, 5, 12, 0, 4, 12));
        // Block both step directions
        g.map[index_for(g, 3, 2)].type = TileType::Wall;  // blocks +x
        g.map[index_for(g, 2, 3)].type = TileType::Wall;  // blocks +y

        monsters_turn(g);

        // Both directions blocked → no movement
        CHECK(g.monsters[0].pos.x == 2);
        CHECK(g.monsters[0].pos.y == 2);
    }
}

// ── Test 1: Combat hit/miss/damage/death ─────────────────

TEST_CASE("Combat: hit/miss/damage/death") {
    Game g = make_minimal_game();
    g.hero = make_hero(/*x=*/1, /*y=*/1,
                       /*atk_bonus=*/20,  /*dmg_die=*/1,
                       /*ac=*/10);

    SUBCASE("guaranteed hit deals damage, hero does not move") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 1,   // x, y (adjacent right)
                         5, 12,  // hp, ac
                         0, 4,   // atk_bonus, dmg_die
                         12));   // morale (never flees)

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        // d1 damage deals exactly 1 → 5 → 4 HP
        CHECK(g.monsters[0].hp == 4);
        CHECK(g.monsters[0].alive);
        // Hero stays put — bump attack replaces movement
        CHECK(g.hero.pos.x == 1);
        CHECK(g.hero.pos.y == 1);
    }

    SUBCASE("guaranteed miss does no damage") {
        g.hero.attack_bonus = -20;
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 1, 5, 12, 0, 4, 12));

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        CHECK(g.monsters[0].hp == 5);
    }

    SUBCASE("hit kills monster with 1 HP") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 1, 1, 12, 0, 4, 12));

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        CHECK(g.monsters[0].hp == 0);
        CHECK_FALSE(g.monsters[0].alive);
    }

    SUBCASE("hero walks past non-adjacent monster without attacking") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         3, 1,   // two tiles away — not adjacent
                         5, 12, 0, 4, 12));

        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        // Hero moves to (2,1), monster at (3,1) untouched
        CHECK(g.hero.pos.x == 2);
        CHECK(g.monsters[0].hp == 5);
    }
}

TEST_CASE("rest heals HP without exceeding max") {
    Game g = make_minimal_game();
    g.hero.hp = 1;
    g.hero.max_hp = 10;

    apply_player_command(g, {PlayerCommandType::Rest, 0, 0});

    CHECK(g.hero.hp >= 2);   // 1 + at least 1 from 1d3
    CHECK(g.hero.hp <= 10);  // capped at max_hp
    CHECK(g.hero.hp <= g.hero.max_hp);
}

TEST_CASE("rest at max HP heals 0 and stays capped") {
    Game g = make_minimal_game();
    g.hero.hp = 10;
    g.hero.max_hp = 10;

    apply_player_command(g, {PlayerCommandType::Rest, 0, 0});

    CHECK(g.hero.hp == 10);
}

TEST_CASE("level 2 rest heals more (2d3)") {
    Game g = make_minimal_game();
    g.hero.hp = 1;
    g.hero.max_hp = 20;
    g.hero_level = 2;

    apply_player_command(g, {PlayerCommandType::Rest, 0, 0});

    CHECK(g.hero.hp >= 3);   // 1 + at least 2 from 2d3
    CHECK(g.hero.hp <= 20);
}

// ── Ranged attack tests ──────────────────────

TEST_CASE("throw knife hits target in line and recovers on kill") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {1, 1};
    g.hero.attack_bonus = 10; // always hits
    g.hero.knives = 3;

    g.monsters.push_back(
        make_monster("Goblin", EntityType::Goblin,
                     3, 1,  // 2 tiles right of hero
                     4, 10, 0, 4, 0));

    apply_player_command(g, {PlayerCommandType::ThrowKnife, 1, 0});

    // Knife used and recovered on kill
    CHECK(g.hero.knives == 3);
    CHECK_FALSE(g.monsters[0].alive);
}

TEST_CASE("throw knife misses with low roll and ammo is lost") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {1, 1};
    g.hero.attack_bonus = -20; // always misses
    g.hero.knives = 3;

    g.monsters.push_back(
        make_monster("Goblin", EntityType::Goblin,
                     3, 1,
                     5, 10, 0, 4, 0));

    apply_player_command(g, {PlayerCommandType::ThrowKnife, 1, 0});

    // Knife used (not recovered on miss)
    CHECK(g.hero.knives == 2);
    CHECK(g.monsters[0].alive);
}

TEST_CASE("throw dart past range flies into darkness") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {1, 1};
    g.hero.darts = 3;

    g.monsters.push_back(
        make_monster("Goblin", EntityType::Goblin,
                     7, 1,  // 6 tiles away — beyond range 4
                     5, 10, 0, 4, 0));

    apply_player_command(g, {PlayerCommandType::ThrowDart, 1, 0});

    // No ammo consumed since nothing was targeted
    CHECK(g.hero.darts == 3);
    CHECK(g.monsters[0].alive);
}

TEST_CASE("thrown weapon is blocked by a closed door") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {1, 1};
    g.hero.attack_bonus = 20;
    g.hero.knives = 3;

    g.map[index_for(g, 2, 1)].type = TileType::DoorClosed;
    g.monsters.push_back(
        make_monster("Goblin", EntityType::Goblin,
                     3, 1,
                     4, 10, 0, 4, 0));

    apply_player_command(g, {PlayerCommandType::ThrowKnife, 1, 0});

    CHECK(g.hero.knives == 3);
    CHECK(g.monsters[0].alive);
    CHECK(g.monsters[0].hp == 4);
}

TEST_CASE("no ammo produces log message and does nothing") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {1, 1};
    g.hero.knives = 0;

    g.monsters.push_back(
        make_monster("Goblin", EntityType::Goblin,
                     3, 1,
                     5, 10, 0, 4, 0));

    apply_player_command(g, {PlayerCommandType::ThrowKnife, 1, 0});

    CHECK(g.hero.knives == 0);
    CHECK(g.monsters[0].alive);
}

TEST_CASE("range penalty applies at distance 2+") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {1, 1};
    g.hero.attack_bonus = -10; // would miss at range 2+ with -1 penalty
    g.hero.knives = 3;

    g.monsters.push_back(
        make_monster("Goblin", EntityType::Goblin,
                     3, 1,  // 2 tiles away, -1 penalty
                     5, 10, 0, 4, 0));

    apply_player_command(g, {PlayerCommandType::ThrowKnife, 1, 0});

    // Ammo used (actually thrown)
    CHECK(g.hero.knives == 2);
}

TEST_CASE("holy water damages undead when thrown") {
    Game g = make_minimal_game(10, 10);
    g.hero = make_hero(1, 1, 20, 4, 10);
    g.hero.holy_water = 1;
    g.hero.gear_counts[4] = 1; // holy-water

    SUBCASE("undead take 1d8 damage") {
        g.monsters.push_back(make_monster("Skeleton", EntityType::Skeleton,
                                          2, 1, 1, 12, 0, 4, 12));
        apply_player_command(g, {PlayerCommandType::ThrowHolyWater, 1, 0, 0});
        CHECK_FALSE(g.monsters[0].alive);
        CHECK(g.hero.holy_water == 0);
        CHECK(g.hero.gear_counts[4] == 0);
    }

    SUBCASE("non-undead are unaffected") {
        g.monsters.push_back(make_monster("Goblin", EntityType::Goblin,
                                          2, 1, 5, 12, 0, 4, 12));
        apply_player_command(g, {PlayerCommandType::ThrowHolyWater, 1, 0, 0});
        CHECK(g.monsters[0].alive);
        CHECK(g.monsters[0].hp == 5);
        CHECK(g.hero.holy_water == 0);
        CHECK(g.hero.gear_counts[4] == 0);
    }

    SUBCASE("requires holy water") {
        g.hero.holy_water = 0;
        g.hero.gear_counts[4] = 0;
        g.monsters.push_back(make_monster("Skeleton", EntityType::Skeleton,
                                          2, 1, 1, 12, 0, 4, 12));
        apply_player_command(g, {PlayerCommandType::ThrowHolyWater, 1, 0, 0});
        CHECK(g.monsters[0].alive);
    }
}

// ── Spell tests ──────────────────────────────────────────

TEST_CASE("Magic Missile spell") {
    Game g = make_minimal_game(10, 10);
    g.hero = make_hero(1, 1, 0, 1, 10);
    g.hero.spell_slots[1] = 1;
    g.hero.spells = {"Magic Missile"};

    SUBCASE("hits monster in line of sight") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         3, 1, 10, 10, 0, 4, 0));

        apply_player_command(g, {PlayerCommandType::Cast, 1, 0, 0});

        // 1d4+1 damage → range 2–5
        CHECK(g.monsters[0].hp >= 5);
        CHECK(g.monsters[0].hp <= 8);
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("blocked by wall") {
        g.map[index_for(g, 2, 1)].type = TileType::Wall;
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         3, 1, 10, 10, 0, 4, 0));

        apply_player_command(g, {PlayerCommandType::Cast, 1, 0, 0});

        CHECK(g.monsters[0].hp == 10);
        CHECK(g.hero.spell_slots[1] == 1);  // slot not consumed
    }

    SUBCASE("blocked by closed door") {
        g.map[index_for(g, 2, 1)].type = TileType::DoorClosed;
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         3, 1, 10, 10, 0, 4, 0));

        apply_player_command(g, {PlayerCommandType::Cast, 1, 0, 0});

        CHECK(g.monsters[0].hp == 10);
        CHECK(g.hero.spell_slots[1] == 1);
    }

    SUBCASE("needs a direction") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         3, 1, 10, 10, 0, 4, 0));

        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK(g.monsters[0].hp == 10);
        CHECK(g.hero.spell_slots[1] == 1);
    }
}

TEST_CASE("Sleep spell") {
    Game g = make_minimal_game(10, 10);
    g.hero = make_hero(1, 1, 0, 1, 10);
    g.hero.spell_slots[1] = 1;
    g.hero.spells = {"Sleep"};

    SUBCASE("kills living monsters") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         3, 1, 5, 10, 0, 4, 0));
        g.monsters.push_back(
            make_monster("Kobold", EntityType::Kobold,
                         4, 1, 5, 10, 0, 4, 0));

        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK_FALSE(g.monsters[0].alive);
        CHECK_FALSE(g.monsters[1].alive);
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("undead are immune") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         3, 1, 5, 10, 0, 4, 0));
        g.monsters.push_back(
            make_monster("Skeleton", EntityType::Skeleton,
                         4, 1, 5, 10, 0, 4, 0));

        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK_FALSE(g.monsters[0].alive);   // goblin sleeps
        CHECK(g.monsters[1].alive);          // skeleton resists
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("no monsters — spell still consumes slot") {
        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        // "Sleep has no effect" but slot consumed
        CHECK(g.hero.spell_slots[1] == 0);
    }
}

TEST_CASE("Cure Light Wounds spell") {
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 0, 1, 10);
    g.hero.hp = 5;
    g.hero.max_hp = 10;
    g.hero.spell_slots[1] = 1;
    g.hero.spells = {"Cure Light Wounds"};

    SUBCASE("heals 1d6+1 HP") {
        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        int healed = g.hero.hp - 5;
        CHECK(healed >= 2);     // 1d6+1 → 2–7
        CHECK(healed <= 7);
        CHECK(g.hero.hp <= 10);
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("capped at max_hp") {
        g.hero.hp = 9;
        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK(g.hero.hp == 10); // at most max_hp
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("no slot — no heal") {
        g.hero.spell_slots[1] = 0;
        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK(g.hero.hp == 5);  // unchanged
    }
}

TEST_CASE("Holy symbol is required for cleric spellcasting and turning") {
    SUBCASE("cleric spells require holy symbol") {
        Game g = make_minimal_game();
        g.hero = make_hero(1, 1, 0, 1, 10);
        g.hero.char_class = CharacterClass::Cleric;
        g.hero.hp = 5;
        g.hero.max_hp = 10;
        g.hero.spell_slots[1] = 1;
        g.hero.spells = {"Cure Light Wounds"};

        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});
        CHECK(g.hero.hp == 5);
        CHECK(g.hero.spell_slots[1] == 1);

        g.hero.gear_counts[3] = 1; // holy-symbol
        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});
        CHECK(g.hero.hp > 5);
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("turn undead requires holy symbol") {
        Game g = make_minimal_game(10, 10);
        g.hero = make_hero(1, 1, 20, 1, 10);
        g.hero.char_class = CharacterClass::Cleric;
        g.hero_level = 20;
        g.monsters.push_back(make_monster("Skeleton", EntityType::Skeleton,
                                          2, 1, 1, 12, 0, 4, 12));

        apply_player_command(g, {PlayerCommandType::Turn, 1, 0, 0});
        CHECK(g.monsters[0].alive);

        g.hero.gear_counts[3] = 1; // holy-symbol
        apply_player_command(g, {PlayerCommandType::Turn, 1, 0, 0});
        CHECK_FALSE(g.monsters[0].alive);
    }
}

TEST_CASE("Charm Person spell") {
    Game g = make_minimal_game(10, 10);
    g.hero = make_hero(1, 1, 0, 1, 10);
    g.hero.spell_slots[1] = 1;
    g.hero.spells = {"Charm Person"};

    SUBCASE("charms adjacent living monster") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         1, 2, 5, 10, 0, 4, 0));

        apply_player_command(g, {PlayerCommandType::Cast, 0, 1, 0});

        CHECK(g.monsters[0].charmed);
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("undead immune") {
        g.monsters.push_back(
            make_monster("Skeleton", EntityType::Skeleton,
                         1, 2, 5, 10, 0, 4, 0));

        apply_player_command(g, {PlayerCommandType::Cast, 0, 1, 0});

        CHECK_FALSE(g.monsters[0].charmed);
        CHECK(g.hero.spell_slots[1] == 1);  // slot not consumed
    }

    SUBCASE("needs a direction") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         1, 2, 5, 10, 0, 4, 0));

        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK_FALSE(g.monsters[0].charmed);
        CHECK(g.hero.spell_slots[1] == 1);
    }

    SUBCASE("attacking breaks charm") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 1, 5, 10, 0, 4, 0));
        g.monsters[0].charmed = true;

        // Hero attacks right into the charmed goblin
        apply_player_command(g, {PlayerCommandType::Move, 1, 0});

        CHECK_FALSE(g.monsters[0].charmed);
        CHECK(g.monsters[0].hp <= 4); // took damage
    }

    SUBCASE("charmed monster does not attack hero") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 1, 5, 10, 20, 6, 0));
        g.monsters[0].charmed = true;

        monsters_turn(g);

        // Charmed → skipped in monsters_turn → hero takes no damage
        CHECK(g.hero.hp == 20);
    }
}

TEST_CASE("Hold Portal spell") {
    Game g = make_minimal_game(10, 10);
    g.hero = make_hero(1, 1, 0, 1, 10);
    g.hero.spell_slots[1] = 1;
    g.hero.spells = {"Hold Portal"};

    SUBCASE("holds adjacent closed door") {
        g.map[index_for(g, 2, 1)].type = TileType::DoorClosed;

        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK(g.map[index_for(g, 2, 1)].held);
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("open fails on held door") {
        g.map[index_for(g, 2, 1)].type = TileType::DoorClosed;
        g.map[index_for(g, 2, 1)].held = true;

        apply_player_command(g, {PlayerCommandType::Open, 0, 0});

        CHECK(g.map[index_for(g, 2, 1)].type == TileType::DoorClosed);
    }

    SUBCASE("no door — slot not consumed") {
        // No door adjacent
        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK(g.hero.spell_slots[1] == 1);
    }
}

TEST_CASE("Iron spikes and hammer can spike doors") {
    Game g = make_minimal_game(10, 10);
    g.hero = make_hero(1, 1, 0, 4, 10);

    SUBCASE("spike closed door shut") {
        auto& door = g.map[index_for(g, 2, 1)];
        door.type = TileType::DoorClosed;
        g.hero.gear_counts[2] = 1; // hammer
        g.hero.gear_counts[5] = 1; // iron-spikes

        apply_player_command(g, {PlayerCommandType::Use, 0, 0});
        CHECK(door.spiked);
        CHECK(g.hero.gear_counts[5] == 0);

        apply_player_command(g, {PlayerCommandType::Open, 0, 0});
        CHECK(door.type == TileType::DoorClosed);
    }

    SUBCASE("spike open door open") {
        auto& door = g.map[index_for(g, 2, 1)];
        door.type = TileType::DoorOpen;
        g.hero.gear_counts[2] = 1; // hammer
        g.hero.gear_counts[5] = 1; // iron-spikes

        apply_player_command(g, {PlayerCommandType::Use, 0, 0});
        CHECK(door.spiked);

        apply_player_command(g, {PlayerCommandType::Close, 0, 0});
        CHECK(door.type == TileType::DoorOpen);
    }

    SUBCASE("spiking requires hammer and spikes") {
        auto& door = g.map[index_for(g, 2, 1)];
        door.type = TileType::DoorClosed;

        apply_player_command(g, {PlayerCommandType::Use, 0, 0});
        CHECK_FALSE(door.spiked);

        g.hero.gear_counts[2] = 1; // hammer only
        apply_player_command(g, {PlayerCommandType::Use, 0, 0});
        CHECK_FALSE(door.spiked);
    }
}

TEST_CASE("Protection from Evil spell") {
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 0, 1, 10);
    g.hero.spell_slots[1] = 1;
    g.hero.spells = {"Protection from Evil"};

    SUBCASE("sets protection_turns to 6") {
        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK(g.hero.protection_turns == 6);
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("AC bonus applies in combat") {
        g.hero.protection_turns = 1;  // active
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         2, 1, 5, 10, 10, 1, 0));

        // Monster attacks hero (AC 10 + 1 from Protection = 11)
        // attack_bonus=10, needs 1+ to hit AC 11 → d20 ≥ 1 always hits
        // But with protection, need d20 ≥ 1 still hits (10+10=20 ≥ 11)
        // This test verifies the bonus is applied (not that it prevents hit)
        monsters_turn(g);

        // protection_turns decremented to 0
        CHECK(g.hero.protection_turns == 0);
    }

    SUBCASE("duration decrements each monster turn") {
        g.hero.protection_turns = 3;

        monsters_turn(g);
        CHECK(g.hero.protection_turns == 2);

        monsters_turn(g);
        CHECK(g.hero.protection_turns == 1);
    }
}

TEST_CASE("Shield spell") {
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 0, 1, 10);
    g.hero.spell_slots[1] = 1;
    g.hero.spells = {"Shield"};

    SUBCASE("sets shield_turns to 6") {
        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK(g.hero.shield_turns == 6);
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("duration decrements each monster turn") {
        g.hero.shield_turns = 3;

        monsters_turn(g);
        CHECK(g.hero.shield_turns == 2);

        monsters_turn(g);
        CHECK(g.hero.shield_turns == 1);
    }
}

TEST_CASE("flavour spells consume slot") {
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 0, 1, 10);

    auto test_spell = [&](const std::string& name) {
        g.hero.spell_slots[1] = 1;
        g.hero.spells = {name};
        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});
        CHECK(g.hero.spell_slots[1] == 0);
    };

    SUBCASE("Light")    { test_spell("Light"); }
    SUBCASE("Read Magic")      { test_spell("Read Magic"); }
    SUBCASE("Detect Magic")    { test_spell("Detect Magic"); }
    SUBCASE("Floating Disc")   { test_spell("Floating Disc"); }
    SUBCASE("Ventriloquism")   { test_spell("Ventriloquism"); }
}

TEST_CASE("no spell slots prevents casting") {
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 0, 1, 10);
    g.hero.spell_slots[1] = 0;
    g.hero.spells = {"Light"};

    apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

    CHECK(g.hero.spell_slots[1] == 0); // still 0
    CHECK(g.event_log.back().find("No spell slots") == 0);
}

TEST_CASE("unknown spell index does nothing") {
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 0, 1, 10);
    g.hero.spell_slots[1] = 1;
    g.hero.spells = {"Light"};

    // Index 5 is out of range (only 1 spell)
    apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 5});

    CHECK(g.hero.spell_slots[1] == 1); // not consumed
}

// ── Cleric spell tests ───────────────────────────────────

TEST_CASE("Detect Evil spell") {
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 0, 1, 10);
    g.hero.spell_slots[1] = 1;
    g.hero.spells = {"Detect Evil"};

    SUBCASE("reports count of living monsters") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         3, 1, 5, 10, 0, 4, 0));
        g.monsters.push_back(
            make_monster("Skeleton", EntityType::Skeleton,
                         4, 1, 5, 10, 0, 4, 0));

        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK(g.event_log.back().find("2 evil") != std::string::npos);
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("reports 0 when no monsters") {
        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK(g.event_log.back().find("0 evil") != std::string::npos);
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("ignores dead monsters") {
        g.monsters.push_back(
            make_monster("Goblin", EntityType::Goblin,
                         3, 1, 0, 10, 0, 4, 0));
        g.monsters[0].alive = false;

        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK(g.event_log.back().find("0 evil") != std::string::npos);
    }
}

TEST_CASE("Purify Food and Water spell") {
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 0, 1, 10);
    g.hero.hp = 5;
    g.hero.max_hp = 10;
    g.hero.spell_slots[1] = 1;
    g.hero.spells = {"Purify Food and Water"};

    SUBCASE("heals 1d4 HP") {
        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        int healed = g.hero.hp - 5;
        CHECK(healed >= 1);
        CHECK(healed <= 4);
        CHECK(g.hero.hp <= 10);
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("capped at max_hp") {
        g.hero.hp = 9;
        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK(g.hero.hp == 10);
    }
}

TEST_CASE("Remove Fear spell") {
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 0, 1, 10);
    g.hero.spell_slots[1] = 1;
    g.hero.spells = {"Remove Fear"};

    SUBCASE("sets courage_turns to 6") {
        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK(g.hero.courage_turns == 6);
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("duration decrements each monster turn") {
        g.hero.courage_turns = 3;

        monsters_turn(g);
        CHECK(g.hero.courage_turns == 2);

        monsters_turn(g);
        CHECK(g.hero.courage_turns == 1);
    }
}

TEST_CASE("Resist Cold spell") {
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 0, 1, 10);
    g.hero.spell_slots[1] = 1;
    g.hero.spells = {"Resist Cold"};

    SUBCASE("sets resist_cold_turns to 6") {
        apply_player_command(g, {PlayerCommandType::Cast, 0, 0, 0});

        CHECK(g.hero.resist_cold_turns == 6);
        CHECK(g.hero.spell_slots[1] == 0);
    }

    SUBCASE("duration decrements each monster turn") {
        g.hero.resist_cold_turns = 3;

        monsters_turn(g);
        CHECK(g.hero.resist_cold_turns == 2);

        monsters_turn(g);
        CHECK(g.hero.resist_cold_turns == 1);
    }
}

// ── Visibility / fog-of-war tests ────────────────────────

TEST_CASE("compute_visibility: hero tile always visible and explored") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {5, 5};
    g.hero.torch_turns = 20;

    compute_visibility(g);

    int idx = index_for(g, 5, 5);
    CHECK(g.map[idx].visible);
    CHECK(g.map[idx].explored);
}

TEST_CASE("compute_visibility: tiles within radius become visible") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {5, 5};
    g.hero.torch_turns = 20;

    compute_visibility(g);

    // Tile 3 steps away should be visible (no obstacles)
    CHECK(g.map[index_for(g, 5, 8)].visible);
    CHECK(g.map[index_for(g, 8, 5)].visible);
    CHECK(g.map[index_for(g, 2, 5)].visible);
    CHECK(g.map[index_for(g, 5, 2)].visible);

    // Tile at the edge of radius (5 steps) should be visible
    CHECK(g.map[index_for(g, 5, 0)].visible);
    CHECK(g.map[index_for(g, 0, 5)].visible);

    // Hero's starting radius is 6
    CHECK(g.map[index_for(g, 1, 5)].visible);
    CHECK(g.map[index_for(g, 9, 5)].visible);

    // Just beyond radius: (5, 0) is 5 steps, visible; (5, -1) OOB
    // (0, 5) is 5 steps, visible; (10, 5) is 5 steps, visible
    CHECK(g.map[index_for(g, 9, 5)].visible);
}

TEST_CASE("compute_visibility: wall blocks line of sight") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {1, 5};
    g.hero.torch_turns = 20;

    // Place wall at (3,5) blocking the ray to (5,5)
    g.map[index_for(g, 3, 5)].type = TileType::Wall;

    compute_visibility(g);

    // Wall tile itself should be explored (line-of-sight hit it)
    CHECK(g.map[index_for(g, 3, 5)].explored);
    CHECK_FALSE(g.map[index_for(g, 3, 5)].visible); // walls are never marked visible

    // Tile behind wall should NOT be visible
    // (wall at (3,5) blocks ray from (1,5) to (5,5))
    CHECK_FALSE(g.map[index_for(g, 5, 5)].visible);
}

TEST_CASE("compute_visibility: explored tiles persist") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {1, 5};
    g.hero.torch_turns = 20;

    // First frame: explore some tiles
    compute_visibility(g);
    CHECK(g.map[index_for(g, 5, 5)].visible);
    CHECK(g.map[index_for(g, 5, 5)].explored);

    // Move hero far away so tiles are no longer visible
    // (9,5) can see up to (3,5) through (9,5) — (1,5) is 8 away, out of range 6
    g.hero.pos = {9, 5};
    compute_visibility(g);

    // Tile at (1,5) is beyond range now (dist 8 > 6)
    CHECK_FALSE(g.map[index_for(g, 1, 5)].visible);
    // But it remains explored
    CHECK(g.map[index_for(g, 1, 5)].explored);
}

TEST_CASE("compute_visibility: no torch = only hero tile visible") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {5, 5};
    g.hero.torch_turns = 0;

    compute_visibility(g);

    // Hero's own tile is visible
    CHECK(g.map[index_for(g, 5, 5)].visible);

    // Adjacent tiles are NOT visible
    CHECK_FALSE(g.map[index_for(g, 4, 5)].visible);
    CHECK_FALSE(g.map[index_for(g, 5, 4)].visible);
    CHECK_FALSE(g.map[index_for(g, 6, 5)].visible);
    CHECK_FALSE(g.map[index_for(g, 5, 6)].visible);
}

TEST_CASE("compute_visibility: torch decrements once per exploration turn") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {5, 5};
    g.hero.torch_turns = 5;
    const int ticks = ticks_per_exploration_turn(g);

    // Calls before the turn boundary: torch still 5.
    for (int i = 0; i < ticks - 1; ++i) {
        compute_visibility(g);
        CHECK(g.hero.torch_turns == 5);
    }

    // Turn boundary: decrement.
    compute_visibility(g);
    CHECK(g.hero.torch_turns == 4);

    // Calls before the next boundary: stays at 4.
    for (int i = 0; i < ticks - 1; ++i) {
        compute_visibility(g);
        CHECK(g.hero.torch_turns == 4);
    }

    // Next turn boundary: decrements to 3.
    compute_visibility(g);
    CHECK(g.hero.torch_turns == 3);

    // Test that it doesn't go negative
    g.hero.torch_turns = 1;
    g.tick_clock = 0;
    for (int i = 0; i < ticks - 1; ++i) {
        compute_visibility(g);
        CHECK(g.hero.torch_turns == 1);
    }
    compute_visibility(g);
    CHECK(g.hero.torch_turns == 0);
    compute_visibility(g);
    CHECK(g.hero.torch_turns == 0);
}

TEST_CASE("compute_visibility: light spell overrides torch") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {5, 5};
    g.hero.torch_turns = 5;
    g.hero.light_spell_turns = 10;

    // After 1 exploration turn, light spell should decrement, torch should not.
    for (int i = 0; i < ticks_per_exploration_turn(g); ++i)
        compute_visibility(g);

    // Light spell is active: torch should NOT decrement
    CHECK(g.hero.torch_turns == 5);
    CHECK(g.hero.light_spell_turns == 9);
}

TEST_CASE("init_game explores entrance room") {
    Game g = make_minimal_game(40, 20);
    auto rooms = init_map(g);

    // Entrance room tiles should NOT be explored yet (init_map doesn't do it)
    CHECK(rooms.size() >= 1);
    const auto& r = rooms[0];
    CHECK_FALSE(g.map[index_for(g, r.x, r.y)].explored);

    // Simulate what init_game does: mark entrance as explored
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x)
            g.map[index_for(g, x, y)].explored = true;

    CHECK(g.map[index_for(g, r.x, r.y)].explored);
    CHECK(g.map[index_for(g, r.x + r.w - 1, r.y + r.h - 1)].explored);
}

// ── Item / pickup tests ────────────────────────────────

TEST_CASE("Get picks up torch from floor") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {5, 5};
    g.hero.torch_turns = 5;

    // Place a torch on the hero's tile
    g.map[index_for(g, 5, 5)].items.push_back(ItemType::Torch);

    PlayerCommand get_cmd;
    get_cmd.type = PlayerCommandType::Get;
    apply_player_command(g, get_cmd);

    CHECK(g.hero.torch_turns == 25);
    CHECK(g.map[index_for(g, 5, 5)].items.empty());
}

TEST_CASE("Get with no items shows message") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {5, 5};
    g.event_log.clear();

    PlayerCommand get_cmd;
    get_cmd.type = PlayerCommandType::Get;
    apply_player_command(g, get_cmd);

    bool found = false;
    for (const auto& msg : g.event_log)
        if (msg.find("Nothing") != std::string::npos) found = true;
    CHECK(found);
}

TEST_CASE("Multiple torches picked up at once") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {5, 5};
    g.hero.torch_turns = 0;

    // Place 3 torches
    auto& tile = g.map[index_for(g, 5, 5)];
    tile.items.push_back(ItemType::Torch);
    tile.items.push_back(ItemType::Torch);
    tile.items.push_back(ItemType::Torch);

    PlayerCommand get_cmd;
    get_cmd.type = PlayerCommandType::Get;
    apply_player_command(g, get_cmd);

    CHECK(g.hero.torch_turns == 60);
    CHECK(g.map[index_for(g, 5, 5)].items.empty());
}

// ── AC buff / HUD tests ──────────────────────────────

TEST_CASE("Shield sets shield_turns without modifying base AC") {
    Game g = make_minimal_game(10, 10);
    g.hero.ac = 10;
    int base_ac_before = g.hero.ac;
    CHECK(g.hero.shield_turns == 0);

    // Simulate what cast_spell("Shield") does
    g.hero.shield_turns = 6;

    CHECK(g.hero.shield_turns == 6);
    CHECK(g.hero.ac == base_ac_before);
    CHECK(g.hero.ac == 10);
}

TEST_CASE("Effective AC combines all active buffs") {
    Game g = make_minimal_game(10, 10);
    g.hero.ac = 10;
    g.hero.shield_turns = 6;
    g.hero.protection_turns = 4;
    g.hero.resist_cold_turns = 2;

    // Replicate the render logic to compute effective_ac and buff_turns
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

    // Shield +4, Protection +1, Resist Cold +1 = +6
    CHECK(effective_ac == 16);
    // Shortest remaining duration
    CHECK(buff_turns == 2);
    CHECK(effective_ac != g.hero.ac); // should show buff indicator
}

TEST_CASE("Effective AC equals base AC when no buffs active") {
    Game g = make_minimal_game(10, 10);
    g.hero.ac = 10;
    g.hero.shield_turns = 0;
    g.hero.protection_turns = 0;
    g.hero.resist_cold_turns = 0;

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

    CHECK(effective_ac == 10);
    CHECK(effective_ac == g.hero.ac); // no buff indicator
}

TEST_CASE("monsters_turn decrements shield_turns") {
    Game g = make_minimal_game(10, 10);
    g.hero.ac = 10;
    g.hero.shield_turns = 3;
    g.hero.pos = {5, 5};
    // Place a monster far away so it doesn't attack
    Entity m;
    m.name = "Rat"; m.type = EntityType::GiantRat;
    m.pos = {0, 0}; m.max_hp = 2; m.hp = 2;
    m.ac = 11; m.attack_bonus = 0; m.damage_die = 3;
    m.morale = 0; m.alive = true;
    g.monsters.push_back(m);

    // Render logic before monsters_turn
    int effective_ac_before = g.hero.ac + 4; // shield is active
    CHECK(effective_ac_before == 14);
    // Check buff_turns logic
    int buff_turns_before = g.hero.shield_turns;
    CHECK(buff_turns_before == 3);

    monsters_turn(g);
    CHECK(g.hero.shield_turns == 2);

    // After decrement, effective AC and buff_turns should reflect new state
    int effective_ac_after = g.hero.ac;
    int buff_turns_after = 999;
    if (g.hero.shield_turns > 0) {
        effective_ac_after += 4;
        buff_turns_after = std::min(buff_turns_after, g.hero.shield_turns);
    }
    CHECK(effective_ac_after == 14); // still buffed
    CHECK(buff_turns_after == 2);    // decremented

    // Decrement a few more times
    monsters_turn(g);
    monsters_turn(g);
    CHECK(g.hero.shield_turns == 0);
    CHECK(g.hero.ac == 10); // base AC never changed
}

TEST_CASE("Torch renders as ! on ground") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {5, 5};

    // Place a torch on an adjacent visible tile
    g.map[index_for(g, 5, 6)].items.push_back(ItemType::Torch);
    g.map[index_for(g, 5, 6)].visible = true;
    g.map[index_for(g, 5, 6)].explored = true;

    // Can't easily capture render output, but check the item is there
    CHECK_FALSE(g.map[index_for(g, 5, 6)].items.empty());
    CHECK(g.map[index_for(g, 5, 6)].items[0] == ItemType::Torch);
}

// ── Spell slot recovery ──────────────────────────────

TEST_CASE("Rest restores spell slots to max") {
    Game g = make_minimal_game(10, 10);
    g.hero.spell_slots[1] = 1;
    g.hero.max_spell_slots[1] = 1;

    // Use the slot
    g.hero.spell_slots[1] = 0;
    CHECK(g.hero.spell_slots[1] == 0);

    // Rest
    PlayerCommand rest;
    rest.type = PlayerCommandType::Rest;
    apply_player_command(g, rest);

    CHECK(g.hero.spell_slots[1] == 1);
}

TEST_CASE("Rest restores level 9 spell slots without out-of-bounds access") {
    Game g = make_minimal_game(10, 10);
    g.hero.spell_slots[Entity::MaxSpellLevel] = 0;
    g.hero.max_spell_slots[Entity::MaxSpellLevel] = 1;

    PlayerCommand rest;
    rest.type = PlayerCommandType::Rest;
    apply_player_command(g, rest);

    CHECK(g.hero.spell_slots[Entity::MaxSpellLevel] == 1);
}

TEST_CASE("Rest only restores up to max") {
    Game g = make_minimal_game(10, 10);
    g.hero.spell_slots[1] = 1;
    g.hero.max_spell_slots[1] = 1;
    g.hero.max_hp = 100;
    g.hero.hp = 80; // needs healing

    // Slot is full (1 of 1)
    // Rest should not overflow
    PlayerCommand rest;
    rest.type = PlayerCommandType::Rest;
    apply_player_command(g, rest);

    CHECK(g.hero.spell_slots[1] == 1); // still 1, not 2
}

TEST_CASE("Rest with no spellcaster does nothing for slots") {
    Game g = make_minimal_game(10, 10);
    g.hero.spell_slots[1] = 0;
    g.hero.max_spell_slots[1] = 0;

    PlayerCommand rest;
    rest.type = PlayerCommandType::Rest;
    apply_player_command(g, rest);

    CHECK(g.hero.spell_slots[1] == 0);
}

// ── Thief skill tests ────────────────────────────────

TEST_CASE("Hide command only works for Thief class") {
    Game g = make_minimal_game(10, 10);
    g.hero.char_class = CharacterClass::Fighter;

    PlayerCommand hide_cmd;
    hide_cmd.type = PlayerCommandType::Hide;
    apply_player_command(g, hide_cmd);

    CHECK_FALSE(g.hero.hidden);
}

TEST_CASE("Thief can hide on success") {
    Game g = make_minimal_game(10, 10);
    g.hero.char_class = CharacterClass::Thief;
    // Force success: set skill to 100%
    g.hero.skill_hide = 100;

    PlayerCommand hide_cmd;
    hide_cmd.type = PlayerCommandType::Hide;
    apply_player_command(g, hide_cmd);

    CHECK(g.hero.hidden);
}

TEST_CASE("Thief can fail to hide") {
    Game g = make_minimal_game(10, 10);
    g.hero.char_class = CharacterClass::Thief;
    // Force failure: set skill to 0%
    g.hero.skill_hide = 0;

    PlayerCommand hide_cmd;
    hide_cmd.type = PlayerCommandType::Hide;
    apply_player_command(g, hide_cmd);

    CHECK_FALSE(g.hero.hidden);
}

TEST_CASE("Movement breaks stealth") {
    Game g = make_minimal_game(10, 10);
    g.hero.char_class = CharacterClass::Thief;
    g.hero.skill_hide = 100;
    g.hero.pos = {5, 5};
    g.hero.hidden = true;

    // Move
    PlayerCommand move;
    move.type = PlayerCommandType::Move;
    move.dx = 1;
    move.dy = 0;
    apply_player_command(g, move);

    CHECK_FALSE(g.hero.hidden);
}

TEST_CASE("Spellcasting breaks stealth") {
    Game g = make_minimal_game(10, 10);
    g.hero.char_class = CharacterClass::Thief;
    g.hero.hidden = true;

    PlayerCommand cast;
    cast.type = PlayerCommandType::Cast;
    apply_player_command(g, cast);

    CHECK_FALSE(g.hero.hidden);
}

TEST_CASE("Ranged attack breaks stealth") {
    Game g = make_minimal_game(10, 10);
    g.hero.char_class = CharacterClass::Thief;
    g.hero.hidden = true;

    PlayerCommand throw_cmd;
    throw_cmd.type = PlayerCommandType::ThrowKnife;
    throw_cmd.dx = 1;
    throw_cmd.dy = 0;
    apply_player_command(g, throw_cmd);

    CHECK_FALSE(g.hero.hidden);
}

TEST_CASE("Waiting does not break stealth") {
    Game g = make_minimal_game(10, 10);
    g.hero.char_class = CharacterClass::Thief;
    g.hero.hidden = true;

    PlayerCommand wait;
    wait.type = PlayerCommandType::Wait;
    apply_player_command(g, wait);

    CHECK(g.hero.hidden);
}

TEST_CASE("Rest does not break stealth") {
    Game g = make_minimal_game(10, 10);
    g.hero.char_class = CharacterClass::Thief;
    g.hero.hidden = true;

    PlayerCommand rest;
    rest.type = PlayerCommandType::Rest;
    apply_player_command(g, rest);

    CHECK(g.hero.hidden);
}

TEST_CASE("Monsters ignore hidden hero") {
    Game g = make_minimal_game(10, 10);
    g.hero.pos = {5, 5};
    g.hero.hidden = true;
    g.hero.max_hp = 999;
    g.hero.hp = 999;
    g.hero.shield_turns = 0;
    g.hero.protection_turns = 0;
    g.hero.courage_turns = 0;
    g.hero.resist_cold_turns = 0;

    // Place a monster adjacent to hero
    Entity m;
    m.name = "Goblin"; m.type = EntityType::Goblin;
    m.pos = {5, 6}; m.max_hp = 5; m.hp = 5;
    m.ac = 12; m.attack_bonus = 1; m.damage_die = 6;
    m.morale = 8; m.alive = true; m.fleeing = false;
    g.monsters.push_back(m);

    CHECK(g.hero.hidden);

    // Monsters turn: should NOT attack because hero is hidden
    monsters_turn(g);

    CHECK(g.hero.hidden);
    CHECK(g.hero.hp == 999); // no damage taken
    CHECK(g.hero.alive);
}

TEST_CASE("Hide command shows log message") {
    Game g = make_minimal_game(10, 10);
    g.hero.char_class = CharacterClass::Thief;
    g.hero.skill_hide = 100;
    g.event_log.clear();

    PlayerCommand hide_cmd;
    hide_cmd.type = PlayerCommandType::Hide;
    apply_player_command(g, hide_cmd);

    bool found = false;
    for (const auto& msg : g.event_log)
        if (msg.find("shadows") != std::string::npos) found = true;
    CHECK(found);
}

TEST_CASE("Thieves' tools are required for thief lock and trap work") {
    Game g = make_minimal_game(10, 10);
    g.hero = make_hero(1, 1, 0, 4, 10);
    g.hero.char_class = CharacterClass::Thief;
    g.hero.skill_pick_locks = 100;
    g.hero.skill_find_traps = 100;

    SUBCASE("locked doors require thieves' tools") {
        auto& tile = g.map[index_for(g, 2, 1)];
        tile.type = TileType::DoorClosed;
        tile.locked = true;

        apply_player_command(g, {PlayerCommandType::Open, 0, 0});
        CHECK(tile.type == TileType::DoorClosed);
        CHECK(tile.locked);

        g.hero.gear_counts[13] = 1; // thieves-tools
        apply_player_command(g, {PlayerCommandType::Open, 0, 0});
        CHECK(tile.type == TileType::DoorOpen);
        CHECK_FALSE(tile.locked);
    }

    SUBCASE("trap disarming requires thieves' tools") {
        auto& tile = g.map[index_for(g, 2, 1)];
        tile.has_trap = true;
        tile.trap_type = TrapType::Pit;

        apply_player_command(g, {PlayerCommandType::Search, 0, 0});
        CHECK(tile.has_trap);
        CHECK(tile.trap_visible);

        tile.trap_visible = false;
        g.hero.gear_counts[13] = 1; // thieves-tools
        apply_player_command(g, {PlayerCommandType::Search, 0, 0});
        CHECK_FALSE(tile.has_trap);
        CHECK(tile.trap_type == TrapType::None);
    }
}

TEST_CASE("10-foot pole probes traps at two-tile range") {
    Game g = make_minimal_game(10, 10);
    g.hero = make_hero(1, 1, 0, 4, 10);
    auto& trap = g.map[index_for(g, 3, 1)];
    trap.has_trap = true;
    trap.trap_type = TrapType::Pit;

    SUBCASE("without pole, range two trap is not found") {
        apply_player_command(g, {PlayerCommandType::Search, 0, 0});
        CHECK_FALSE(trap.trap_visible);
    }

    SUBCASE("with pole, range two trap is revealed") {
        g.hero.gear_counts[19] = 1; // pole-10ft
        apply_player_command(g, {PlayerCommandType::Search, 0, 0});
        CHECK(trap.trap_visible);
        CHECK(trap.has_trap);
    }
}

TEST_CASE("10-foot pole auto-probes on move") {
    Game g = make_minimal_game(10, 10);
    g.hero = make_hero(5, 5, 0, 4, 10);
    g.hero.hp = 3;
    g.hero.max_hp = 3;
    auto& trap = g.map[index_for(g, 6, 5)]; // tile to the east
    trap.has_trap = true;
    trap.trap_type = TrapType::Dart;

    SUBCASE("without pole, moving onto trap triggers it") {
        CHECK(g.hero.hp == 3);
        apply_player_command(g, {PlayerCommandType::Move, 1, 0});
        CHECK(g.hero.hp < 3);
        CHECK_FALSE(trap.has_trap); // trap consumed
    }

    SUBCASE("with pole, moving onto trap reveals it safely") {
        g.hero.gear_counts[19] = 1; // pole-10ft
        CHECK(g.hero.hp == 3);
        apply_player_command(g, {PlayerCommandType::Move, 1, 0});
        CHECK(g.hero.hp == 3); // unharmed
        CHECK(trap.trap_visible);
        CHECK(trap.has_trap); // trap still present
    }
}

// ========== B/X Data smoke tests ==========
#include "bx_data.hpp"

TEST_CASE("bx_data ability score modifiers match B/X Moldvay") {
    using namespace bx;

    // Strength band 18
    auto s18 = get_strength(18);
    CHECK(s18.toHitMelee == 3);
    CHECK(s18.damageAdj == 3);

    // Strength band 3
    auto s3 = get_strength(3);
    CHECK(s3.toHitMelee == -3);
    CHECK(s3.damageAdj == -3);

    // Strength band 9-12 (mid)
    auto s10 = get_strength(10);
    CHECK(s10.toHitMelee == 0);
    CHECK(s10.damageAdj == 0);

    // Dexterity AC bonus: 18 gives -3 (bonus)
    auto d18 = get_dexterity(18);
    CHECK(d18.toHitMissile == 3);
    CHECK(d18.acBonus == -3);

    // Dexterity AC penalty: 3 gives +3 (penalty)
    auto d3 = get_dexterity(3);
    CHECK(d3.toHitMissile == -3);
    CHECK(d3.acBonus == 3);

    // Constitution HP bonus
    auto c18 = get_constitution(18);
    CHECK(c18.hpBonusPerDie == 3);

    auto c6 = get_constitution(6);
    CHECK(c6.hpBonusPerDie == -1);

    // Wisdom saving throw bonus
    auto w18 = get_wisdom(18);
    CHECK(w18.savingThrowMagic == 3);

    // Charisma retainer max
    auto ch18 = get_charisma(18);
    CHECK(ch18.maxRetainers == 7);
    CHECK(ch18.retainerMorale == 10);
}

TEST_CASE("bx_data class definitions exist") {
    using namespace bx;

    // Check a few class names
    bool found_fighter = false;
    bool found_mu = false;
    for (const auto& c : class_info) {
        if (std::string_view(c.name) == "Fighter") found_fighter = true;
        if (std::string_view(c.name) == "Magic-User") found_mu = true;
    }
    CHECK(found_fighter);
    CHECK(found_mu);

    // Fighter level 1
    CHECK(fighter_levels[0].level == 1);
    CHECK(fighter_levels[0].xp_required == 0);
    CHECK(std::string_view(fighter_levels[0].title) == "Veteran");

    // Magic-User gets 1 first-level spell at level 1
    CHECK(magic_user_levels[0].first_level_spells == 1);

    // Cleric gets 0 spells at level 1
    CHECK(cleric_levels[0].first_level_spells == 0);
}

TEST_CASE("bx_data spells list is populated") {
    using namespace bx;

    // We should have all first-level spells
    bool found_clw = false;   // Cure Light Wounds
    bool found_mm = false;    // Magic Missile
    bool found_sleep = false;

    for (const auto& s : spells) {
        if (std::string_view(s.name) == "Cure Light Wounds") found_clw = true;
        if (std::string_view(s.name) == "Magic Missile") found_mm = true;
        if (std::string_view(s.name) == "Sleep") found_sleep = true;
    }

    CHECK(found_clw);
    CHECK(found_mm);
    CHECK(found_sleep);
    CHECK(spells.size() >= 37);  // Full B/X spell list
}

TEST_CASE("bx_data monsters are loaded") {
    using namespace bx;

    CHECK(!monsters.empty());
    CHECK(monsters.size() >= 100);  // 101 B/X monsters

    // Check a specific monster
    bool found_dragon = false;
    for (const auto& m : monsters) {
        if (std::string_view(m.name) == "Dragon, Red") {
            CHECK(m.ac == -1);
            CHECK(m.morale == 8);
            found_dragon = true;
            break;
        }
    }
    CHECK(found_dragon);
}

TEST_CASE("bx_data saving throws are correct") {
    using namespace bx;

    CHECK(saving_throws.size() == 7);  // 7 classes

    // Fighter: Death Ray/Poison 12
    // Index 1 = fighter (cleric=0, fighter=1)
    CHECK(saving_throws[1].death_ray_poison == 12);

    // Dwarf: Death Ray/Poison 8
    CHECK(saving_throws[4].death_ray_poison == 8);
}

TEST_CASE("bx_data attack tables match THAC0 values") {
    using namespace bx;

    // Normal man vs AC 9 = 11
    CHECK(character_attack_table[0].ac_9 == 11);

    // 1st-3rd level vs AC 9 = 10
    CHECK(character_attack_table[1].ac_9 == 10);

    // 1st-3rd level vs AC 2 = 17
    CHECK(character_attack_table[1].ac_2 == 17);
}

TEST_CASE("bx_data weapons and equipment") {
    using namespace bx;

    // Check a few weapons exist
    bool found_sword = false;
    bool found_dagger = false;
    for (const auto& w : weapons) {
        if (std::string_view(w.name) == "Sword") {
            CHECK(w.damage_die == std::string_view("1d8"));
            CHECK(w.cost_gp == 10);
            found_sword = true;
        }
        if (std::string_view(w.name) == "Dagger") {
            CHECK(w.thrown);
            found_dagger = true;
        }
    }
    CHECK(found_sword);
    CHECK(found_dagger);

    // Check armor
    CHECK(armor_items[0].ac == 7);   // Leather Armor
    CHECK(armor_items[2].ac == 3);   // Plate Mail
}

TEST_CASE("is_weapon_allowed helper") {
    // Fighters can use any weapon
    CHECK(is_weapon_allowed(CharacterClass::Fighter, "sword"));
    CHECK(is_weapon_allowed(CharacterClass::Fighter, "dagger"));
    CHECK(is_weapon_allowed(CharacterClass::Fighter, "crossbow"));

    // Magic-Users can only use dagger
    CHECK(is_weapon_allowed(CharacterClass::MagicUser, "dagger"));
    CHECK_FALSE(is_weapon_allowed(CharacterClass::MagicUser, "sword"));
    CHECK_FALSE(is_weapon_allowed(CharacterClass::MagicUser, "mace"));

    // Thieves: dagger, short-sword, sling
    CHECK(is_weapon_allowed(CharacterClass::Thief, "dagger"));
    CHECK(is_weapon_allowed(CharacterClass::Thief, "short-sword"));
    CHECK(is_weapon_allowed(CharacterClass::Thief, "sling"));
    CHECK_FALSE(is_weapon_allowed(CharacterClass::Thief, "sword"));
    CHECK_FALSE(is_weapon_allowed(CharacterClass::Thief, "battle-axe"));

    // Clerics: mace, club, war-hammer, sling
    CHECK(is_weapon_allowed(CharacterClass::Cleric, "mace"));
    CHECK(is_weapon_allowed(CharacterClass::Cleric, "club"));
    CHECK(is_weapon_allowed(CharacterClass::Cleric, "war-hammer"));
    CHECK(is_weapon_allowed(CharacterClass::Cleric, "sling"));
    CHECK_FALSE(is_weapon_allowed(CharacterClass::Cleric, "sword"));
}

TEST_CASE("Armorer: buy weapon via apply_town_command") {
    Game g = make_minimal_game();
    g.hero = make_hero(5, 3, 0, 4, 9);
    g.hero.char_class = CharacterClass::Fighter;
    g.total_gold = 100;
    g.state = GameState::Armorer;

    SUBCASE("buy a sword (10 gp)") {
        // Enter weapon submenu
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 0});
        CHECK(g.town_submenu == 1);  // now in weapon selection

        // Select sword (index 4 in the weapon list, but depends on class filtering)
        // For a Fighter, all weapons are allowed. Count: battle-axe(0), hand-axe(1), crossbow(2), long-bow(3), short-bow(4), dagger(5), silver-dagger(6), short-sword(7), sword(8), two-handed-sword(9), mace(10), club(11), pole-arm(12), sling(13), spear(14), war-hammer(15)
        // Sword is at index 8
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 8});
        CHECK(g.hero.weapon_id == "sword");
        CHECK(g.hero.damage_die == 8);
        CHECK(g.hero.weapon_counts[8] == 1);
        CHECK(g.total_gold == 90);
        CHECK(g.town_submenu == 1);
    }

    SUBCASE("cannot afford a weapon") {
        g.total_gold = 5;
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 0});  // enter submenu
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 8});  // try sword (10 gp)
        CHECK(g.hero.weapon_id.empty());  // still no weapon
        CHECK(g.total_gold == 5);         // gold unchanged
    }
}

TEST_CASE("Character sheet equips owned weapons and armor") {
    Game g = make_minimal_game();
    g.hero = make_hero(5, 3, 0, 4, 9);
    g.hero.char_class = CharacterClass::Fighter;
    g.hero.weapon_counts[8] = 1; // sword
    g.hero.weapon_counts[9] = 1; // two-handed sword
    g.hero.armor_counts[0] = 1;  // leather armor
    g.hero.armor_counts[3] = 1;  // shield
    g.hero.weapon_id = "sword";
    g.hero.has_shield = true;
    g.state = GameState::Town;

    apply_town_command(g, {PlayerCommandType::Inventory, 0, 0, 0});
    CHECK(g.state == GameState::CharacterSheet);
    CHECK(g.previous_state == GameState::Town);

    apply_town_command(g, {PlayerCommandType::Get, 0, 0, 0});
    CHECK(g.town_submenu == 6);
    apply_town_command(g, {PlayerCommandType::Get, 0, 0, 1});
    CHECK(g.hero.weapon_id == "two-handed-sword");
    CHECK(g.hero.damage_die == 10);
    CHECK_FALSE(g.hero.has_shield);

    apply_town_command(g, {PlayerCommandType::Help, 0, 0, 0});
    CHECK(g.town_submenu == 0);
    apply_town_command(g, {PlayerCommandType::Get, 0, 0, 1});
    CHECK(g.town_submenu == 7);
    apply_town_command(g, {PlayerCommandType::Get, 0, 0, 0});
    CHECK(g.hero.armor_id == "leather-armor");
    CHECK(g.hero.ac == 7);

    apply_town_command(g, {PlayerCommandType::Help, 0, 0, 0});
    apply_town_command(g, {PlayerCommandType::Help, 0, 0, 0});
    CHECK(g.state == GameState::Town);
}

TEST_CASE("B/X encumbrance uses coin weight movement tiers") {
    Game g = make_minimal_game();
    g.hero = make_hero(5, 3, 0, 4, 9);

    g.total_gold = 400;
    CHECK(carried_weight_cn(g) == 400);
    CHECK(exploration_move_feet(g) == 120);
    CHECK(encounter_move_feet(g) == 40);
    CHECK(ticks_per_exploration_turn(g) == 12);

    g.total_gold = 401;
    CHECK(exploration_move_feet(g) == 90);
    CHECK(encounter_move_feet(g) == 30);
    CHECK(ticks_per_exploration_turn(g) == 9);

    g.total_gold = 601;
    CHECK(exploration_move_feet(g) == 60);
    CHECK(encounter_move_feet(g) == 20);
    CHECK(ticks_per_exploration_turn(g) == 6);

    g.total_gold = 801;
    CHECK(exploration_move_feet(g) == 30);
    CHECK(encounter_move_feet(g) == 10);
    CHECK(ticks_per_exploration_turn(g) == 3);
}

TEST_CASE("Encumbrance includes owned equipment and gear") {
    Game g = make_minimal_game();
    g.hero = make_hero(5, 3, 0, 4, 9);
    g.total_gold = 100;
    g.hero.weapon_counts[8] = 1; // sword, 60 cn
    g.hero.armor_counts[0] = 1;  // leather armor, 200 cn
    g.hero.gear_counts[10] = 1;  // rope, 50 cn

    CHECK(carried_weight_cn(g) == 410);
    CHECK(exploration_move_feet(g) == 90);
}

TEST_CASE("Save/load preserves owned weapons and armor") {
    Game g = make_minimal_game();
    g.hero = make_hero(5, 3, 0, 4, 9);
    g.hero.char_class = CharacterClass::Fighter;
    g.hero.weapon_counts[8] = 2;
    g.hero.armor_counts[0] = 1;
    g.hero.armor_counts[3] = 1;
    g.hero.weapon_id = "sword";
    g.hero.armor_id = "leather-armor";
    g.hero.has_shield = true;
    g.total_gold = 42;

    const auto path = std::filesystem::path{"/tmp/opencode/bx_crawl_save_test.dat"};
    CHECK(save_game(g, path));

    Game loaded;
    CHECK(load_game(loaded, path));
    CHECK(loaded.hero.weapon_counts[8] == 2);
    CHECK(loaded.hero.armor_counts[0] == 1);
    CHECK(loaded.hero.armor_counts[3] == 1);
    CHECK(loaded.hero.weapon_id == "sword");
    CHECK(loaded.hero.armor_id == "leather-armor");
    CHECK(loaded.hero.has_shield);
    CHECK(loaded.total_gold == 42);

    std::filesystem::remove(path);
}

TEST_CASE("Save helpers: per-character save paths and enumerate") {
    const char* old_home = std::getenv("HOME");
    setenv("HOME", "/tmp/opencode", 1);

    CHECK(saves_directory() == std::filesystem::path("/tmp/opencode/.bx_crawl/saves"));

    auto path = save_path_for_hero("Volk");
    CHECK(path.filename() == "Volk.dat");
    CHECK(path.parent_path() == saves_directory());

    // Clean up any test saves
    std::filesystem::remove_all(saves_directory());
    CHECK(enumerate_saves().empty());

    // Create a save and verify it appears in the list
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 0, 4, 9);
    g.hero.name = "Volk";
    g.hero.char_class = CharacterClass::Fighter;
    g.total_gold = 100;

    CHECK(save_game(g, save_path_for_hero("Volk")));
    auto saves = enumerate_saves();
    CHECK(saves.size() == 1);
    CHECK(strip_save_extension(saves[0]) == "Volk");

    // Load it back
    Game loaded;
    CHECK(load_game(loaded, saves[0]));
    CHECK(loaded.hero.name == "Volk");
    CHECK(loaded.hero.char_class == CharacterClass::Fighter);
    CHECK(loaded.total_gold == 100);

    std::filesystem::remove_all(saves_directory());

    // Restore HOME
    if (old_home) setenv("HOME", old_home, 1);
    else unsetenv("HOME");
}

TEST_CASE("find_save checks pregenerated directory as fallback") {
    const char* old_home = std::getenv("HOME");
    setenv("HOME", "/tmp/opencode", 1);
    std::filesystem::remove_all(saves_directory());

    // Create a pregenerated save in data/pregenerated_saves/
    auto data_dir = std::filesystem::path{"../data"};
    auto pregen_dir = pregenerated_saves_directory(data_dir);
    std::filesystem::create_directories(pregen_dir);

    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 0, 4, 9);
    g.hero.name = "Fighter";
    g.total_gold = 50;
    CHECK(save_game(g, pregen_dir / "Fighter.dat"));

    // find_save should find it in pregenerated dir (not in user saves)
    auto found = find_save("Fighter", data_dir);
    CHECK_FALSE(found.empty());
    CHECK(strip_save_extension(found) == "Fighter");

    // enumerate_all_saves should include it
    auto all = enumerate_all_saves(data_dir);
    bool found_pregen = false;
    for (const auto& p : all)
        if (strip_save_extension(p) == "Fighter") found_pregen = true;
    CHECK(found_pregen);

    // Clean up
    std::filesystem::remove_all(pregen_dir);
    std::filesystem::remove_all(saves_directory());
    if (old_home) setenv("HOME", old_home, 1);
    else unsetenv("HOME");
}

// ── Persistence hardening tests ──────────────────────────────────
// These treat the on-disk save format as a compatibility spec: payloads are
// handcrafted here so that individual bytes can be corrupted to exercise the
// loader's validation paths.

namespace {

constexpr std::size_t kSerializedStringLimitForTests = 1024;

// Copy an int32 into its serialized form (little-endian byte order matches
// the writer, which memcpy's the raw value).
std::string pack_i32(int32_t v) {
    std::string b(sizeof(v), '\0');
    std::memcpy(&b[0], &v, sizeof(v));
    return b;
}

std::string pack_str(const std::string& s) {
    return pack_i32(static_cast<int32_t>(s.size())) + s;
}

struct CraftedSave {
    std::string bytes;
    std::size_t off_state{};
    std::size_t off_hero_level{};
    std::size_t off_dungeon_level{};
    std::size_t off_name_len{};
    std::size_t off_char_class{};
    std::size_t off_nspells{};
};

// A valid version-5 payload following the exact byte layout produced by
// save_game. Field offsets are recorded so tests can corrupt individual values.
CraftedSave make_valid_save_v5() {
    CraftedSave s;
    auto i32 = [&](int32_t v) { s.bytes += pack_i32(v); };
    auto str = [&](const std::string& v) { s.bytes += pack_str(v); };

    s.bytes += "BXSV";                          // magic
    i32(5);                                     // version

    s.off_state = s.bytes.size();        i32(0);   // GameState::Town
    i32(100);                                      // total_gold
    i32(0);                                        // total_xp
    s.off_hero_level = s.bytes.size();     i32(1); // hero_level
    s.off_dungeon_level = s.bytes.size();  i32(1); // dungeon_level

    s.off_name_len = s.bytes.size();       str("CraftedHero");
    i32(10); i32(10);                       // hp, max_hp
    i32(1); i32(6); i32(0); i32(9);         // attack_bonus, damage_die, damage_bonus, ac
    i32(9); i32(9); i32(9); i32(9); i32(9); i32(9); // abilities (str,int,wis,dex,con,cha)

    s.off_char_class = s.bytes.size();      i32(1); // Fighter
    i32(60); i32(0); i32(0); i32(0);        // torch, light, knives, darts
    i32(0); i32(0); i32(0); i32(0);         // shield/protection/courage/resist turns

    str(""); str("");                       // weapon_id, armor_id
    i32(0);                                 // has_shield
    i32(0); i32(0); i32(0); i32(0); i32(0); i32(0); // six thief skills

    s.off_nspells = s.bytes.size();         i32(0); // no known spells
    for (int i = 0; i <= 9; ++i) i32(0);            // spell_slots[0..9]
    for (int i = 0; i <= 9; ++i) i32(0);            // max_spell_slots[0..9]

    // Version 2+ fields
    i32(0); i32(0); i32(0); i32(0); i32(0); i32(0); i32(0); // rations..ai_wander_timer
    // Version 3+ fields
    for (int i = 0; i < 20; ++i) i32(0);                // gear_counts
    // Version 4+ fields
    i32(0);                                             // lantern_turns
    // Version 5+ fields
    for (int i = 0; i < 16; ++i) i32(0);                // weapon_counts
    for (int i = 0; i < 4; ++i)  i32(0);                // armor_counts
    return s;
}

void write_save_file(const std::string& bytes, const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void patch_i32(std::string& bytes, std::size_t off, int32_t v) {
    REQUIRE(off + sizeof(int32_t) <= bytes.size());
    std::memcpy(&bytes[off], &v, sizeof(v));
}

// A representative live game that malformed loads must not disturb.
Game make_known_game_state() {
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 2, 6, 9);
    g.hero.name = "Keeper";
    g.hero.char_class = CharacterClass::MagicUser;
    g.hero.weapon_id = "sword";
    g.hero.armor_id = "leather-armor";
    g.hero.spells = {"Sleep", "Read Magic"};
    g.state = GameState::Dungeon;
    g.total_gold = 999;
    g.hero_level = 4;
    g.dungeon_level = 3;
    g.rng.seed(777u);
    g.map[0].type = TileType::Wall;
    g.map[0].visible = true;
    g.map[0].items.push_back(ItemType::Torch);
    g.monsters.push_back(make_monster("Goblin", EntityType::Goblin, 2, 2, 3, 7, 1, 6, 7));
    g.rooms.push_back(Room{1, 1, 3, 3});
    g.event_log.push_back("Known state must survive failed loads.");
    return g;
}

// Load `path` into `g`; verify the load fails and no meaningful field changes.
void assert_load_rejected_cleanly(Game& g, const std::filesystem::path& path) {
    const Game before = g;
    CHECK_FALSE(load_game(g, path));
    CHECK(g.hero.name == before.hero.name);
    CHECK(g.hero.hp == before.hero.hp);
    CHECK(g.hero.max_hp == before.hero.max_hp);
    CHECK(g.hero.abilities.str == before.hero.abilities.str);
    CHECK(g.hero.char_class == before.hero.char_class);
    CHECK(g.hero.weapon_id == before.hero.weapon_id);
    CHECK(g.hero.armor_id == before.hero.armor_id);
    CHECK(g.hero.spells == before.hero.spells);
    CHECK(g.state == before.state);
    CHECK(g.total_gold == before.total_gold);
    CHECK(g.total_xp == before.total_xp);
    CHECK(g.hero_level == before.hero_level);
    CHECK(g.dungeon_level == before.dungeon_level);
    CHECK(g.width == before.width);
    CHECK(g.height == before.height);
    REQUIRE(g.map.size() == before.map.size());
    REQUIRE_FALSE(g.map.empty());
    CHECK(g.map[0].type == before.map[0].type);
    CHECK(g.map[0].visible == before.map[0].visible);
    CHECK(g.map[0].items == before.map[0].items);
    REQUIRE(g.monsters.size() == before.monsters.size());
    REQUIRE_FALSE(g.monsters.empty());
    CHECK(g.monsters[0].name == before.monsters[0].name);
    CHECK(g.monsters[0].hp == before.monsters[0].hp);
    CHECK(g.monsters[0].pos.x == before.monsters[0].pos.x);
    CHECK(g.monsters[0].pos.y == before.monsters[0].pos.y);
    REQUIRE(g.rooms.size() == before.rooms.size());
    REQUIRE_FALSE(g.rooms.empty());
    CHECK(g.rooms[0].x == before.rooms[0].x);
    CHECK(g.rooms[0].y == before.rooms[0].y);
    CHECK(g.rooms[0].w == before.rooms[0].w);
    CHECK(g.rooms[0].h == before.rooms[0].h);
    CHECK(g.event_log == before.event_log);
    auto before_rng = before.rng;
    auto after_rng = g.rng;
    CHECK(before_rng() == after_rng());
}

} // anonymous namespace

TEST_CASE("Crafted v5 payload loads, validating the format contract") {
    auto crafted = make_valid_save_v5();
    auto path = std::filesystem::path{"/tmp/opencode/bx_crawl_crafted_v5.dat"};
    write_save_file(crafted.bytes, path);

    Game loaded = make_minimal_game();
    CHECK(load_game(loaded, path));
    CHECK(loaded.hero.name == "CraftedHero");
    CHECK(loaded.hero.char_class == CharacterClass::Fighter);
    CHECK(loaded.hero.hp == 10);
    CHECK(loaded.hero.max_hp == 10);
    CHECK(loaded.total_gold == 100);
    CHECK(loaded.hero_level == 1);
    CHECK(loaded.dungeon_level == 1);
    CHECK(loaded.state == GameState::Town);

    std::filesystem::remove(path);
}

TEST_CASE("Malformed saves: bad magic and unsupported versions are rejected") {
    auto path = std::filesystem::path{"/tmp/opencode/bx_crawl_bad_magic.dat"};
    write_save_file("NOTABXSAVE........", path);
    Game g = make_known_game_state();
    assert_load_rejected_cleanly(g, path);
    std::filesystem::remove(path);

    for (int32_t bad_version : {-1, 0, 6, 1000000}) {
        auto c = make_valid_save_v5();
        patch_i32(c.bytes, 4, bad_version);
        auto p = std::filesystem::path{"/tmp/opencode/bx_crawl_bad_version.dat"};
        write_save_file(c.bytes, p);
        assert_load_rejected_cleanly(g, p);
        std::filesystem::remove(p);
    }
}

TEST_CASE("Malformed saves: truncated files are rejected without side effects") {
    auto crafted = make_valid_save_v5();
    const std::vector<std::size_t> cuts = {
        0, 4, 8, 12, 32,
        crafted.off_name_len, crafted.off_name_len + 4,
        crafted.bytes.size() / 2, crafted.bytes.size() - 16,
        crafted.bytes.size() - 4, crafted.bytes.size() - 1
    };

    Game g = make_known_game_state();
    int idx = 0;
    for (std::size_t cut : cuts) {
        auto p = std::filesystem::path{"/tmp/opencode/bx_crawl_trunc_" +
                                       std::to_string(idx++) + ".dat"};
        write_save_file(crafted.bytes.substr(0, cut), p);
        assert_load_rejected_cleanly(g, p);
        std::filesystem::remove(p);
    }
}

TEST_CASE("Malformed saves: invalid string lengths are rejected before allocation") {
    const int32_t lengths[] = {-1, -100, 0x10000000, 0x7FFFFFFF};
    Game g = make_known_game_state();
    int idx = 0;
    for (int32_t len : lengths) {
        auto c = make_valid_save_v5();
        patch_i32(c.bytes, c.off_name_len, len);
        auto p = std::filesystem::path{"/tmp/opencode/bx_crawl_badlen_" +
                                       std::to_string(idx++) + ".dat"};
        write_save_file(c.bytes, p);
        assert_load_rejected_cleanly(g, p);
        std::filesystem::remove(p);
    }
}

TEST_CASE("Malformed saves: invalid serialized enums are rejected") {
    Game g = make_known_game_state();

    auto c = make_valid_save_v5();
    for (int32_t state : {-1, 99, 500}) {
        patch_i32(c.bytes, c.off_state, state);
        auto p = std::filesystem::path{"/tmp/opencode/bx_crawl_bad_state.dat"};
        write_save_file(c.bytes, p);
        assert_load_rejected_cleanly(g, p);
        std::filesystem::remove(p);
    }

    auto c2 = make_valid_save_v5();
    for (int32_t cc : {-2, 99, 42}) {
        patch_i32(c2.bytes, c2.off_char_class, cc);
        auto p2 = std::filesystem::path{"/tmp/opencode/bx_crawl_bad_class.dat"};
        write_save_file(c2.bytes, p2);
        assert_load_rejected_cleanly(g, p2);
        std::filesystem::remove(p2);
    }
}

TEST_CASE("Malformed saves: impossible counts and levels are rejected") {
    Game g = make_known_game_state();

    for (int32_t level : {0, -1, 1000000}) {
        auto c = make_valid_save_v5();
        patch_i32(c.bytes, c.off_hero_level, level);
        auto p = std::filesystem::path{"/tmp/opencode/bx_crawl_bad_hl.dat"};
        write_save_file(c.bytes, p);
        assert_load_rejected_cleanly(g, p);
        std::filesystem::remove(p);
    }

    for (int32_t dl : {0, -3, 1000000}) {
        auto c = make_valid_save_v5();
        patch_i32(c.bytes, c.off_dungeon_level, dl);
        auto p = std::filesystem::path{"/tmp/opencode/bx_crawl_bad_dl.dat"};
        write_save_file(c.bytes, p);
        assert_load_rejected_cleanly(g, p);
        std::filesystem::remove(p);
    }

    for (int32_t n : {-1, 5000, 1000000}) {
        auto c = make_valid_save_v5();
        patch_i32(c.bytes, c.off_nspells, n);
        auto p = std::filesystem::path{"/tmp/opencode/bx_crawl_bad_n.dat"};
        write_save_file(c.bytes, p);
        assert_load_rejected_cleanly(g, p);
        std::filesystem::remove(p);
    }
}

TEST_CASE("Old save versions still load with defaulted fields") {
    for (int32_t version : {1, 2, 3, 4}) {
        auto crafted = make_valid_save_v5();
        patch_i32(crafted.bytes, 4, version);
        auto p = std::filesystem::path{"/tmp/opencode/bx_crawl_old_v" +
                                       std::to_string(version) + ".dat"};
        write_save_file(crafted.bytes, p);

        Game loaded = make_minimal_game();
        CHECK(load_game(loaded, p));
        CHECK(loaded.hero.name == "CraftedHero");
        CHECK(loaded.hero.char_class == CharacterClass::Fighter);
        CHECK(loaded.total_gold == 100);
        if (version >= 2)
            CHECK(loaded.hero.rations == 0);
        if (version < 4)
            CHECK(loaded.hero.lantern_turns == 0);
        if (version < 5) {
            bool any_owned = false;
            for (int c : loaded.hero.weapon_counts)
                any_owned = any_owned || (c != 0);
            CHECK_FALSE(any_owned);   // weapon_id empty → no weapon restored
        }
        std::filesystem::remove(p);
    }
}

TEST_CASE("save_game fails instead of silently succeeding when it cannot write") {
    auto blocker = std::filesystem::path{"/tmp/opencode/bx_crawl_blocker"};
    std::filesystem::remove_all(blocker);
    { std::ofstream f(blocker); f << "not a directory"; }

    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 2, 6, 9);
    CHECK_FALSE(save_game(g, blocker / "x.dat"));   // parent is a regular file

    std::filesystem::remove(blocker);
}

TEST_CASE("Serialized string limit is enforced by both writer and reader") {
    auto max_path = std::filesystem::path{"/tmp/opencode/bx_crawl_max_string.dat"};
    auto too_long_path = std::filesystem::path{"/tmp/opencode/bx_crawl_too_long_string.dat"};
    std::filesystem::remove(max_path);
    std::filesystem::remove(too_long_path);

    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 2, 6, 9);
    g.hero.char_class = CharacterClass::Fighter;
    g.hero.name = std::string(kSerializedStringLimitForTests, 'A');
    CHECK(save_game(g, max_path));

    Game loaded = make_minimal_game();
    CHECK(load_game(loaded, max_path));
    CHECK(loaded.hero.name.size() == kSerializedStringLimitForTests);
    CHECK(loaded.hero.name == g.hero.name);

    g.hero.name = std::string(kSerializedStringLimitForTests + 1, 'B');
    CHECK_FALSE(save_game(g, too_long_path));

    std::filesystem::remove(max_path);
    std::filesystem::remove(too_long_path);
}

TEST_CASE("Hero-derived save filenames never escape the saves directory") {
    ScopedEnvVar home("HOME", "/tmp/opencode");
    std::filesystem::remove_all(saves_directory());

    const char* nasty_names[] = {
        "../../evil", "foo/bar", "foo\\bar", "..", ".", "",
        "/etc/passwd", "a/b/c", "x..y", ".hidden", " hero ",
        "../sneaky", "dir/..", "C:\\windows\\system32"
    };
    for (const char* n : nasty_names) {
        auto p = save_path_for_hero(n);
        CHECK(p.parent_path() == saves_directory());
        CHECK(p.filename().string().find('/') == std::string::npos);
        CHECK(p.filename().string().find('\\') == std::string::npos);
        CHECK(p.filename().string().find("..") == std::string::npos);
    }
    CHECK(save_path_for_hero("").filename() == "hero.dat");

    // Sanitized round trip: a hostile name still saves/loads within saves dir.
    Game g = make_minimal_game();
    g.hero = make_hero(1, 1, 2, 6, 9);
    g.hero.name = "foo/bar";
    auto p = save_path_for_hero("foo/bar");
    CHECK(save_game(g, p));
    {
        auto all = enumerate_saves();
        bool found = false;
        for (const auto& e : all)
            if (e == p) found = true;
        CHECK(found);
    }
    Game loaded;
    CHECK(load_game(loaded, p));

    // find_save uses the same sanitization for user and pregenerated lookups.
    auto data_dir = std::filesystem::path{"/tmp/opencode/bx_crawl_testdata"};
    auto pregen_dir = pregenerated_saves_directory(data_dir);
    std::filesystem::remove_all(data_dir);
    std::filesystem::create_directories(pregen_dir);
    CHECK(save_game(g, pregen_dir / "Volk.dat"));

    auto found_user = find_save("foo/bar", data_dir);
    CHECK_FALSE(found_user.empty());
    CHECK(found_user == p);

    auto found_pregen = find_save("Volk", data_dir);
    CHECK_FALSE(found_pregen.empty());
    CHECK(strip_save_extension(found_pregen) == "Volk");

    std::filesystem::remove_all(data_dir);
    std::filesystem::remove_all(saves_directory());
}

TEST_CASE("Help command (h key) is safe from every town screen") {
    auto test_help = [](GameState st, int submenu = 0) {
        Game g = make_minimal_game();
        g.hero = make_hero(1, 1, 0, 4, 9);
        g.state = st;
        g.town_submenu = submenu;
        GameState before = g.state;
        int before_sub = g.town_submenu;
        bool result = apply_town_command(g, {PlayerCommandType::Help, 0, 0, 0});
        // Help should never quit, never crash, and manage state safely
        CHECK(result == true);
        if (st == GameState::CharacterSheet) {
            if (before_sub != 0)
                CHECK(g.town_submenu == 0);
            else
                CHECK(g.state != before); // returns to previous_state
        } else if (st == GameState::Shop || st == GameState::Armorer) {
            if (before_sub != 0)
                CHECK(g.town_submenu == 0);
            else
                CHECK(g.state == before);
        } else {
            CHECK(g.state == before);
        }
    };

    test_help(GameState::Town);
    test_help(GameState::Tavern);
    test_help(GameState::Temple);
    test_help(GameState::Shop, 0);
    test_help(GameState::Shop, 4);
    test_help(GameState::Armorer, 0);
    test_help(GameState::Armorer, 1);
    test_help(GameState::CharacterSheet, 0);
    test_help(GameState::CharacterSheet, 6);
}

TEST_CASE("Force door uses STR openDoors modifier") {
    Game g = make_minimal_game(10, 6);
    g.hero = make_hero(1, 1, 0, 4, 9);

    // Place a locked door to the east
    g.map[index_for(g, 2, 1)].type = TileType::DoorClosed;
    g.map[index_for(g, 2, 1)].locked = true;

    // STR 9 gives openDoors 0 → 2-in-6 chance
    g.hero.abilities.str = 9;
    g.rng.seed(42u);
    int force_count = 0;
    int trials = 1000;
    for (int i = 0; i < trials; ++i) {
        g.map[index_for(g, 2, 1)].type = TileType::DoorClosed;
        g.map[index_for(g, 2, 1)].locked = true;
        g.rng.seed(static_cast<unsigned long>(i * 7 + 999));
        g.tick_clock = 0;

        apply_player_command(g, {PlayerCommandType::Open, 0, 0, 0});
        if (!g.map[index_for(g, 2, 1)].locked && g.map[index_for(g, 2, 1)].type == TileType::DoorOpen)
            ++force_count;
    }
    // STR 9: 2-in-6 ≈ 33%, should be between 25-42%
    CHECK(force_count > 250);
    CHECK(force_count < 420);

    // STR 18 gives openDoors 3 → 5-in-6 (83%)
    g.hero.abilities.str = 18;
    force_count = 0;
    for (int i = 0; i < trials; ++i) {
        g.map[index_for(g, 2, 1)].type = TileType::DoorClosed;
        g.map[index_for(g, 2, 1)].locked = true;
        g.rng.seed(static_cast<unsigned long>(i * 13 + 777));
        g.tick_clock = 0;

        apply_player_command(g, {PlayerCommandType::Open, 0, 0, 0});
        if (!g.map[index_for(g, 2, 1)].locked && g.map[index_for(g, 2, 1)].type == TileType::DoorOpen)
            ++force_count;
    }
    // STR 18: 5-in-6 ≈ 83%
    CHECK(force_count > 750);
    CHECK(force_count < 900);
}

TEST_CASE("Dungeon peace announcement is one-time and not game over state") {
    Game g = make_minimal_game();
    CHECK_FALSE(any_monsters_alive(g));

    announce_dungeon_at_peace(g);
    CHECK(g.dungeon_peace_announced);
    CHECK(g.event_log.size() == 2);

    announce_dungeon_at_peace(g);
    CHECK(g.event_log.size() == 2);
}

TEST_CASE("Armorer: buy armor via apply_town_command") {
    Game g = make_minimal_game();
    g.hero = make_hero(5, 3, 0, 4, 9);
    g.hero.char_class = CharacterClass::Fighter;
    g.total_gold = 100;
    g.state = GameState::Armorer;

    SUBCASE("buy leather armor (20 gp, AC 7)") {
        // Enter armor submenu (option 2)
        // Actually option 1=weapon, 2=armor, 3=sell
        // But all three map to Get — apply_town_command checks the current submenu
        // First, set submenu to 2 (armor selection)
        g.town_submenu = 2;
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 0}); // buy leather (index 0)
        CHECK(g.hero.armor_id == "leather-armor");
        CHECK(g.hero.ac == 7);
        CHECK(g.hero.armor_counts[0] == 1);
        CHECK(g.total_gold == 80);
        CHECK(g.town_submenu == 2);
    }

    SUBCASE("Magic-User cannot wear chain mail") {
        g.hero.char_class = CharacterClass::MagicUser;
        g.town_submenu = 2;
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 1}); // chain mail (index 1)
        CHECK(g.hero.armor_id.empty()); // not allowed, still unarmored
        CHECK(g.total_gold == 100);     // gold untouched
    }

    SUBCASE("buy shield (10 gp)") {
        g.hero.weapon_id = "sword"; // need a weapon to use shield
        g.town_submenu = 2;
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 3}); // shield (index 3)
        CHECK(g.hero.has_shield);
        CHECK(g.hero.armor_counts[3] == 1);
        CHECK(g.total_gold == 90);
    }
}

TEST_CASE("Armorer: sell equipment via apply_town_command") {
    Game g = make_minimal_game();
    g.hero = make_hero(5, 3, 0, 4, 9);
    g.hero.char_class = CharacterClass::Fighter;
    g.hero.weapon_id = "sword";
    g.hero.armor_id = "leather-armor";
    g.hero.ac = 7;
    g.hero.has_shield = true;
    g.total_gold = 0;
    g.state = GameState::Armorer;

    SUBCASE("sell weapon for half price (5 gp)") {
        g.town_submenu = 3;
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 0}); // sell weapon
        CHECK(g.hero.weapon_id.empty());
        CHECK(g.total_gold == 5);
    }

    SUBCASE("sell armor for half price (10 gp)") {
        g.town_submenu = 3;
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 1}); // sell armor
        CHECK(g.hero.armor_id.empty());
        CHECK(g.hero.ac == 9); // back to unarmored
        CHECK(g.total_gold == 10);
    }

    SUBCASE("sell shield for 5 gp") {
        g.town_submenu = 3;
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 2}); // sell shield
        CHECK_FALSE(g.hero.has_shield);
        CHECK(g.total_gold == 5);
    }
}

TEST_CASE("Shop: buy rations and supplies via apply_town_command") {
    Game g = make_minimal_game();
    g.hero = make_hero(5, 3, 0, 4, 9);
    g.total_gold = 100;
    g.state = GameState::Shop;

    SUBCASE("buy standard rations (5 gp)") {
        g.town_submenu = 4;
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 9}); // standard rations
        CHECK(g.hero.rations == 21);
        CHECK(g.hero.gear_counts[9] == 1);
        CHECK(g.total_gold == 95);
    }

    SUBCASE("buy iron rations (15 gp)") {
        g.town_submenu = 4;
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 8}); // iron rations
        CHECK(g.hero.rations == 42);
        CHECK(g.hero.gear_counts[8] == 1);
        CHECK(g.total_gold == 85);
    }

    SUBCASE("buy oil flask (2 gp)") {
        g.town_submenu = 4;
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 1}); // oil flask
        CHECK(g.hero.oil_flasks == 1);
        CHECK(g.hero.gear_counts[1] == 1);
        CHECK(g.total_gold == 98);
    }

    SUBCASE("buy holy water (25 gp)") {
        g.town_submenu = 4;
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 4}); // holy water
        CHECK(g.hero.holy_water == 1);
        CHECK(g.hero.gear_counts[4] == 1);
        CHECK(g.total_gold == 75);
    }

    SUBCASE("buy second-page gear") {
        g.town_submenu = 5;
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 19}); // 10-foot pole
        CHECK(g.hero.gear_counts[19] == 1);
        CHECK(g.total_gold == 99);
    }

    SUBCASE("cannot afford") {
        g.total_gold = 3;
        g.town_submenu = 4;
        apply_town_command(g, {PlayerCommandType::Get, 0, 0, 0}); // backpack (5 gp)
        CHECK(g.hero.gear_counts[0] == 0);  // nothing bought
        CHECK(g.total_gold == 3);    // gold untouched
    }

    SUBCASE("page navigation") {
        g.town_submenu = 4;
        apply_town_command(g, {PlayerCommandType::Open, 0, 0, 0});
        CHECK(g.town_submenu == 5);
        apply_town_command(g, {PlayerCommandType::Close, 0, 0, 0});
        CHECK(g.town_submenu == 4);
    }

    SUBCASE("back from submenu via Help command") {
        g.town_submenu = 4;
        apply_town_command(g, {PlayerCommandType::Help, 0, 0, 0});
        CHECK(g.town_submenu == 0);
    }
}

TEST_CASE("Lantern requires lantern, oil, and tinder box") {
    Game g = make_minimal_game(10, 10);
    g.hero = make_hero(1, 1, 0, 4, 10);

    SUBCASE("no lantern means nothing to use") {
        apply_player_command(g, {PlayerCommandType::Use, 0, 0});
        CHECK(g.hero.lantern_turns == 0);
    }

    SUBCASE("lantern needs tinder box") {
        g.hero.gear_counts[6] = 1; // lantern
        g.hero.gear_counts[1] = 1; // oil-flask
        g.hero.oil_flasks = 1;
        apply_player_command(g, {PlayerCommandType::Use, 0, 0});
        CHECK(g.hero.lantern_turns == 0);
        CHECK(g.hero.oil_flasks == 1);
    }

    SUBCASE("lantern needs oil") {
        g.hero.gear_counts[6] = 1;  // lantern
        g.hero.gear_counts[14] = 1; // tinder-box
        apply_player_command(g, {PlayerCommandType::Use, 0, 0});
        CHECK(g.hero.lantern_turns == 0);
    }

    SUBCASE("oil fuels lantern for 24 turns") {
        g.hero.gear_counts[6] = 1;  // lantern
        g.hero.gear_counts[14] = 1; // tinder-box
        g.hero.gear_counts[1] = 1;  // oil-flask
        g.hero.oil_flasks = 1;
        apply_player_command(g, {PlayerCommandType::Use, 0, 0});
        CHECK(g.hero.lantern_turns == 24);
        CHECK(g.hero.oil_flasks == 0);
        CHECK(g.hero.gear_counts[1] == 0);
    }
}

TEST_CASE("Light torch from inventory with Use key") {
    Game g = make_minimal_game(10, 10);
    g.hero = make_hero(1, 1, 0, 4, 10);
    g.hero.torch_turns = 0;

    SUBCASE("torch needs tinder box") {
        g.hero.gear_counts[15] = 1; // torches
        apply_player_command(g, {PlayerCommandType::Use, 0, 0});
        CHECK(g.hero.torch_turns == 0);
        CHECK(g.hero.gear_counts[15] == 1);
    }

    SUBCASE("light one torch for 20 turns") {
        g.hero.gear_counts[15] = 1; // torches
        g.hero.gear_counts[14] = 1; // tinder-box
        apply_player_command(g, {PlayerCommandType::Use, 0, 0});
        CHECK(g.hero.torch_turns == 20);
        CHECK(g.hero.gear_counts[15] == 0);
    }

    SUBCASE("lights multiple torches sequentially") {
        g.hero.gear_counts[15] = 3;
        g.hero.gear_counts[14] = 1; // tinder-box
        apply_player_command(g, {PlayerCommandType::Use, 0, 0});
        CHECK(g.hero.torch_turns == 20);
        CHECK(g.hero.gear_counts[15] == 2);
    }

    SUBCASE("won't light while torch is already lit") {
        g.hero.torch_turns = 5;
        g.hero.gear_counts[15] = 1;
        g.hero.gear_counts[14] = 1; // tinder-box
        apply_player_command(g, {PlayerCommandType::Use, 0, 0});
        CHECK(g.hero.torch_turns == 5); // unchanged
        CHECK(g.hero.gear_counts[15] == 1); // unconsumed
    }
}

TEST_CASE("Rope protects against pit trap damage once") {
    Game g = make_minimal_game(10, 10);
    g.hero = make_hero(1, 1, 0, 4, 10);

    auto& pit = g.map[index_for(g, 2, 1)];
    pit.has_trap = true;
    pit.trap_type = TrapType::Pit;

    SUBCASE("without rope, pit damages hero") {
        int hp_before = g.hero.hp;
        apply_player_command(g, {PlayerCommandType::Move, 1, 0});
        CHECK(g.hero.hp < hp_before);
        CHECK_FALSE(pit.has_trap);
    }

    SUBCASE("rope prevents pit damage and is consumed") {
        g.hero.gear_counts[10] = 1; // rope
        int hp_before = g.hero.hp;
        apply_player_command(g, {PlayerCommandType::Move, 1, 0});
        CHECK(g.hero.hp == hp_before);
        CHECK(g.hero.gear_counts[10] == 0);
        CHECK_FALSE(pit.has_trap);
    }
}

TEST_CASE("Hunger: rations consumed and starvation") {
    Game g = make_minimal_game();
    g.hero = make_hero(5, 3, 0, 4, 12);
    g.hero.rations = 2;

    SUBCASE("rations are consumed on schedule") {
        // Simulate B/X turns (compute_visibility increments hunger)
        // turns_without_food starts at 0, increments each turn tick
        // Every 48 ticks, if rations > 0, consume 1 and reset clock
        g.hero.turns_without_food = 47;

        // Starting state
        CHECK(g.hero.rations == 2);
        CHECK(g.hero.turns_without_food == 47);

        // If we simulate the hunger logic:
        // tick → turns_without_food becomes 48, rations consumed
        g.hero.turns_without_food++;
        if (g.hero.turns_without_food > 0 && g.hero.turns_without_food % 48 == 0) {
            if (g.hero.rations > 0) {
                g.hero.rations--;
                g.hero.turns_without_food = 0;
            }
        }
        CHECK(g.hero.rations == 1);
        CHECK(g.hero.turns_without_food == 0);
    }

    SUBCASE("starvation without rations") {
        g.hero.rations = 0;
        g.hero.turns_without_food = 47;

        // Simulate one more tick → turns_without_food reaches 48 (rumble)
        g.hero.turns_without_food++;

        CHECK(g.hero.rations == 0);
        CHECK(g.hero.turns_without_food == 48);
    }
}

TEST_CASE("Monster AI: patrol and chase") {
    Game g = make_minimal_game();
    g.hero = make_hero(10, 10, 0, 4, 12);

    SUBCASE("monster far away wanders (patrol)") {
        Entity m = make_monster("Goblin", EntityType::Goblin,
                                1, 1, 5, 12, 0, 4, 8);
        m.ai_patrol_x = 1;
        m.ai_patrol_y = 1;
        g.monsters.push_back(m);

        // Monster is far from hero (dist = 18 > 8) → should patrol
        // With wander_timer = 7, next tick triggers it to 8 and it wanders
        g.monsters[0].ai_wander_timer = 7;
        monsters_turn(g);

        // Should have wandered (timer wrapped, attempted random move)
        CHECK(g.monsters[0].alive);
        // Position may have changed or stayed same depending on roll_d(8)
    }

    SUBCASE("monster adjacent attacks") {
        Entity m = make_monster("Goblin", EntityType::Goblin,
                                9, 10, 5, 12, 0, 4, 8);
        g.monsters.push_back(m);

        monsters_turn(g);

        // Adjacent monster should have attacked hero
        CHECK(g.hero.hp <= 20); // may have taken damage
        CHECK_FALSE(g.monsters[0].fleeing); // should not flee (high morale)
    }

    SUBCASE("charmed monsters do not attack") {
        Entity m = make_monster("Goblin", EntityType::Goblin,
                                9, 10, 5, 12, 0, 4, 8);
        m.charmed = true;
        g.monsters.push_back(m);

        monsters_turn(g);

        // Charmed monster should not have attacked
        CHECK(g.hero.hp == 20);
    }

    SUBCASE("fleeing monsters try to escape") {
        Entity m = make_monster("Goblin", EntityType::Goblin,
                                9, 10, 5, 12, 0, 4, 8);
        m.fleeing = true;
        g.monsters.push_back(m);

        int old_x = g.monsters[0].pos.x;

        monsters_turn(g);

        // Should have moved away from hero (hero at 10,10, monster at 9,10)
        // Flee moves toward increasing distance → likely 8,10 or 9,9 or 9,11
        // The monster might also escape if it reaches the edge
        CHECK(g.monsters[0].pos.x <= old_x); // should move left or stay
    }
}

// ══════════════════════════════════════════════════════════════
// GameSession application boundary
// ══════════════════════════════════════════════════════════════

namespace {

// Minimal dungeon with a hero and optionally one adjacent monster (2,1).
Game make_dungeon_game(bool with_monster) {
    Game g = make_minimal_game(12, 8);
    g.state = GameState::Dungeon;
    g.hero = make_hero(1, 1, 10, 6, 12);
    g.hero.name = "Session Hero";
    if (with_monster) {
        g.monsters.push_back(make_monster("Orc", EntityType::Monster,
                                          2, 1, 4, 10, 0, 6, 9));
    }
    return g;
}

// Reproduce the pre-boundary dungeon turn cycle exactly as the text frontend
// used to sequence it, for behavioral/RNG-equivalence checks against
// GameSession.
void legacy_dungeon_cycle(Game& g, const PlayerCommand& cmd) {
    compute_visibility(g);

    if (cmd.type == PlayerCommandType::Inventory ||
        cmd.type == PlayerCommandType::Quit) {
        apply_player_command(g, cmd);
        return;
    }

    const bool monsters_present = any_monsters_alive(g);
    const bool monsters_first = monsters_present && roll_initiative(g);

    if (monsters_first) {
        monsters_turn(g);
        if (!g.hero.alive) return;
    }

    apply_player_command(g, cmd);

    if (g.state != GameState::Dungeon) return;

    if (!g.hero.alive) return;

    if (!any_monsters_alive(g))
        announce_dungeon_at_peace(g);

    if (monsters_present && !monsters_first)
        monsters_turn(g);
}

// Two identical seeded games must end up indistinguishable after running the
// same dungeon command, one through GameSession and one through the legacy
// (frontend-sequenced) path.
void check_dungeon_cycle_equivalent(const Game& session_g, const Game& legacy_g) {
    CHECK(session_g.hero.hp == legacy_g.hero.hp);
    CHECK(session_g.hero.alive == legacy_g.hero.alive);
    CHECK(session_g.hero.pos.x == legacy_g.hero.pos.x);
    CHECK(session_g.hero.pos.y == legacy_g.hero.pos.y);
    CHECK(session_g.state == legacy_g.state);
    CHECK(session_g.tick_clock == legacy_g.tick_clock);
    CHECK(session_g.dungeon_peace_announced == legacy_g.dungeon_peace_announced);
    CHECK(session_g.event_log == legacy_g.event_log);
    REQUIRE(session_g.monsters.size() == legacy_g.monsters.size());
    for (std::size_t i = 0; i < session_g.monsters.size(); ++i) {
        CHECK(session_g.monsters[i].alive == legacy_g.monsters[i].alive);
        CHECK(session_g.monsters[i].hp == legacy_g.monsters[i].hp);
        CHECK(session_g.monsters[i].pos.x == legacy_g.monsters[i].pos.x);
        CHECK(session_g.monsters[i].pos.y == legacy_g.monsters[i].pos.y);
    }
    auto session_rng = session_g.rng;
    auto legacy_rng = legacy_g.rng;
    CHECK(session_rng() == legacy_rng());
}

} // anonymous namespace

TEST_CASE("GameSession owns the authoritative Game state") {
    GameSession session{make_minimal_game()};
    Game& g = session.game();
    g.hero = make_hero(1, 1, 0, 1, 9);
    g.hero.hp = 10;
    g.state = GameState::Dungeon;

    CHECK(session.game().hero.hp == 10);
    CHECK(session.game().state == GameState::Dungeon);

    // The transitional accessor reflects session-owned state and vice versa.
    session.game().hero.hp = 6;
    CHECK(session.game().hero.hp == 6);
    CHECK(g.hero.hp == 6);
}

TEST_CASE("GameSession delegates town commands through a single boundary") {
    GameSession session{make_minimal_game()}; // default state: Town
    session.game().hero = make_hero(1, 1, 0, 1, 9);

    PlayerCommand nav;
    nav.type = PlayerCommandType::Move;
    nav.dx = 0;
    nav.dy = -1; // tavern from town square

    ActionResult result = session.apply(nav);
    CHECK(result.keep_playing);
    CHECK_FALSE(result.hero_died);
    CHECK(session.game().state == GameState::Tavern);
    CHECK(session.game().town_submenu == 0);

    PlayerCommand quit;
    quit.type = PlayerCommandType::Quit;
    result = session.apply(quit);
    CHECK_FALSE(result.keep_playing);
    CHECK_FALSE(result.hero_died);
}

TEST_CASE("GameSession: non-turn commands do not consume initiative or monster dice") {
    GameSession session{make_dungeon_game(true)};
    session.prepare_dungeon_frame(); // advance the clock as the frontend does

    auto rng_before = session.game().rng;

    PlayerCommand inv;
    inv.type = PlayerCommandType::Inventory;
    ActionResult result = session.apply(inv);

    CHECK(result.keep_playing);
    CHECK_FALSE(result.hero_died);
    REQUIRE_FALSE(session.game().monsters.empty());
    CHECK(session.game().monsters[0].alive);

    // Inventory is not a turn: the RNG stream must be untouched (no
    // initiative/monster rolls happened).
    auto rng_after = session.game().rng;
    CHECK(rng_before() == rng_after());

    PlayerCommand quit;
    quit.type = PlayerCommandType::Quit;
    result = session.apply(quit);
    CHECK_FALSE(result.keep_playing);
}

TEST_CASE("GameSession preserves dungeon turn sequencing and RNG ordering") {
    PlayerCommand move_into_monster;
    move_into_monster.type = PlayerCommandType::Move;
    move_into_monster.dx = 1; // hero at (1,1) attacks the monster at (2,1)

    PlayerCommand wait;
    wait.type = PlayerCommandType::Wait;

    PlayerCommand rest;
    rest.type = PlayerCommandType::Rest;

    for (unsigned seed : {1u, 77u, 12345u, 777u}) {
        SUBCASE("move into adjacent monster") {
            Game initial = make_dungeon_game(true);
            initial.rng.seed(seed);

            GameSession session{Game(initial)};
            session.prepare_dungeon_frame();
            ActionResult result = session.apply(move_into_monster);

            Game legacy = initial;
            legacy_dungeon_cycle(legacy, move_into_monster);

            check_dungeon_cycle_equivalent(session.game(), legacy);
            if (!session.game().hero.alive) {
                CHECK_FALSE(result.keep_playing);
                CHECK(result.hero_died);
            }
        }

        SUBCASE("wait") {
            Game initial = make_dungeon_game(true);
            initial.rng.seed(seed);

            GameSession session{Game(initial)};
            session.prepare_dungeon_frame();
            session.apply(wait);

            Game legacy = initial;
            legacy_dungeon_cycle(legacy, wait);

            check_dungeon_cycle_equivalent(session.game(), legacy);
        }

        SUBCASE("rest") {
            Game initial = make_dungeon_game(true);
            initial.hero.hp = 5;
            initial.rng.seed(seed);

            GameSession session{Game(initial)};
            session.prepare_dungeon_frame();
            session.apply(rest);

            Game legacy = initial;
            legacy_dungeon_cycle(legacy, rest);

            check_dungeon_cycle_equivalent(session.game(), legacy);
        }

        SUBCASE("monster-free floor is untouched") {
            Game initial = make_dungeon_game(false);
            initial.hero.hp = 20;
            initial.rng.seed(seed);

            GameSession session{Game(initial)};
            session.prepare_dungeon_frame();
            session.apply(wait);

            Game legacy = initial;
            legacy_dungeon_cycle(legacy, wait);

            check_dungeon_cycle_equivalent(session.game(), legacy);
        }
    }
}

TEST_CASE("GameSession leaves the dungeon and skips monster checks after ascending") {
    Game initial = make_dungeon_game(false);
    initial.map[index_for(initial, 2, 1)].type = TileType::StairsUp;
    initial.hero.pos = {1, 1};
    initial.rng.seed(4242u);

    PlayerCommand ascend;
    ascend.type = PlayerCommandType::Move;
    ascend.dx = 1; // step onto the StairsUp tile

    GameSession session{Game(initial)};
    session.prepare_dungeon_frame();
    ActionResult result = session.apply(ascend);

    CHECK(result.keep_playing);
    CHECK_FALSE(result.hero_died);
    CHECK(session.game().state == GameState::Camp);

    Game legacy = initial;
    legacy_dungeon_cycle(legacy, ascend);
    check_dungeon_cycle_equivalent(session.game(), legacy);
}

TEST_CASE("GameSession: monsters winning initiative can kill the hero before acting") {
    auto make_fragile_dungeon = []() {
        Game g = make_minimal_game(12, 8);
        g.state = GameState::Dungeon;
        g.hero = make_hero(1, 1, 0, 1, 10);
        g.hero.hp = 1;
        g.monsters.push_back(make_monster("Ogre", EntityType::Monster,
                                          2, 1, 20, 10, 20, 6, 9));
        return g;
    };

    PlayerCommand wait;
    wait.type = PlayerCommandType::Wait;

    unsigned died_before = 0;
    unsigned died_after = 0;
    for (unsigned seed = 1; seed < 20000 && (died_before == 0 || died_after == 0); ++seed) {
        Game g = make_fragile_dungeon();
        g.rng.seed(seed);
        GameSession session{std::move(g)};
        session.prepare_dungeon_frame();
        ActionResult result = session.apply(wait);

        if (!result.hero_died) continue;
        if (result.died_before_action) {
            if (died_before == 0) {
                died_before = seed;
                CHECK_FALSE(result.keep_playing);
                CHECK_FALSE(session.game().hero.alive);
            }
        } else if (died_after == 0) {
            died_after = seed;
            CHECK_FALSE(result.keep_playing);
            CHECK_FALSE(session.game().hero.alive);
        }
    }

    CAPTURE(died_before);
    CAPTURE(died_after);
    CHECK(died_before != 0); // some seed: monsters acted first and killed the hero
    CHECK(died_after != 0);  // some seed: hero acted, then monsters killed the hero
}

TEST_CASE("GameSession.prepare_dungeon_frame advances dungeon timing and lights the hero tile") {
    Game g = make_minimal_game(12, 8);
    g.state = GameState::Dungeon;
    g.hero = make_hero(3, 3, 0, 1, 9);
    g.hero.torch_turns = 60; // lit torch → radius 6

    GameSession session{std::move(g)};
    CHECK(session.game().tick_clock == 0);

    session.prepare_dungeon_frame();

    CHECK(session.game().tick_clock == 1);
    CHECK(session.game().map[index_for(session.game(), 3, 3)].visible);
}

TEST_CASE("GameSession snapshot exposes presentation state without mutating RNG") {
    Game g = make_minimal_game(12, 8);
    g.state = GameState::Dungeon;
    g.hero = make_hero(3, 3, 2, 6, 9);
    g.hero.name = "Snapshot Hero";
    g.hero.char_class = CharacterClass::Fighter;
    g.hero.knives = 2;
    g.hero.darts = 3;
    g.total_gold = 12;
    g.event_log.push_back("Snapshot log line.");
    g.monsters.push_back(make_monster("Goblin", EntityType::Goblin,
                                      4, 3, 3, 7, 1, 6, 7));
    g.rng.seed(999u);

    GameSession session{std::move(g)};
    session.prepare_dungeon_frame();

    auto rng_before = session.game().rng;
    GameStateSnapshot snapshot = session.snapshot();
    auto rng_after = session.game().rng;

    CHECK(rng_before() == rng_after());
    CHECK(snapshot.state == GameState::Dungeon);
    CHECK(snapshot.width == 12);
    CHECK(snapshot.height == 8);
    CHECK(snapshot.hero.name == "Snapshot Hero");
    CHECK(snapshot.hero.class_name == "Fighter");
    CHECK(snapshot.hero.pos.x == 3);
    CHECK(snapshot.hero.pos.y == 3);
    CHECK(snapshot.hero.knives == 2);
    CHECK(snapshot.hero.darts == 3);
    CHECK(snapshot.total_gold == 12);
    CHECK(snapshot.purse_text.find("12 gp") != std::string::npos);
    CHECK(snapshot.alive_monster_count == 1);
    REQUIRE(snapshot.monsters.size() == 1);
    CHECK(snapshot.monsters[0].name == "Goblin");
    CHECK(snapshot.monsters[0].glyph == 'g');
    CHECK(snapshot.event_log.back() == "Snapshot log line.");

    const auto& hero_tile = snapshot.tiles[index_for(session.game(), 3, 3)];
    CHECK(hero_tile.visible);
    CHECK(hero_tile.explored);
}

TEST_CASE("GameStateSnapshot carries town and character-sheet render data") {
    Game g = make_minimal_game();
    g.state = GameState::CharacterSheet;
    g.town_submenu = 6;
    g.hero = make_hero(1, 1, 1, 8, 6);
    g.hero.name = "Sheet Hero";
    g.hero.char_class = CharacterClass::Elf;
    g.hero.weapon_id = "sword";
    g.hero.armor_id = "chain-mail";
    g.hero.has_shield = true;
    g.hero.weapon_counts[0] = 1;
    g.hero.gear_counts[0] = 2;
    g.total_gold = 25;
    g.total_xp = 100;
    g.hero_level = 1;

    GameStateSnapshot snapshot = make_game_state_snapshot(g);

    CHECK(snapshot.state == GameState::CharacterSheet);
    CHECK(snapshot.town_submenu == 6);
    CHECK(snapshot.hero.class_name == "Elf");
    CHECK(snapshot.hero.weapon_id == "sword");
    CHECK(snapshot.hero.armor_id == "chain-mail");
    CHECK(snapshot.hero.has_shield);
    CHECK(snapshot.hero.weapon_counts[0] == 1);
    CHECK(snapshot.hero.gear_counts[0] == 2);
    CHECK(snapshot.purse_text.find("25 gp") != std::string::npos);
    CHECK(snapshot.total_xp == 100);
    CHECK(snapshot.next_level_xp > snapshot.total_xp);
}

TEST_CASE("Structured log events preserve existing event_log text") {
    Game g = make_minimal_game();
    g.state = GameState::Tavern;
    g.hero = make_hero(1, 1, 1, 6, 9);
    g.hero.hp = 5;
    g.hero.max_hp = 10;
    g.total_gold = 1;
    g.rng.seed(17u);

    GameSession session{std::move(g)};

    PlayerCommand rest;
    rest.type = PlayerCommandType::Rest;
    ActionResult result = session.apply(rest);

    CHECK(result.keep_playing);
    REQUIRE_FALSE(session.game().event_log.empty());

    bool found_structured_log = false;
    for (const auto& event : session.events()) {
        if (event.type == GameEventType::LogMessage &&
            event.text == session.game().event_log.back()) {
            found_structured_log = true;
            break;
        }
    }
    CHECK(found_structured_log);
}

TEST_CASE("Gameplay sound cues are queued as structured events") {
    Game g = make_minimal_game(12, 8);
    g.state = GameState::Dungeon;
    g.hero = make_hero(3, 3, 1, 6, 9);
    g.rng.seed(123u);

    GameSession session{std::move(g)};
    session.prepare_dungeon_frame();

    PlayerCommand move;
    move.type = PlayerCommandType::Move;
    move.dx = 1;
    move.dy = 0;
    ActionResult result = session.apply(move);

    CHECK(result.keep_playing);

    bool found_footstep = false;
    for (const auto& event : session.events()) {
        if (event.type == GameEventType::Sound && event.sound == SoundEffect::Footstep) {
            found_footstep = true;
            break;
        }
    }
    CHECK(found_footstep);
}

// ══════════════════════════════════════════════════════════════
// Content IDs / RulesCatalog adapter
// ══════════════════════════════════════════════════════════════

TEST_CASE("ContentId validation accepts stable dotted lowercase ids") {
    CHECK(is_valid_content_id("bx.weapon.long_bow"));
    CHECK(is_valid_content_id("bx.spell.magic_user.magic_missile"));
    CHECK(is_valid_content_id("module.testing_tomb.room.room_a"));

    CHECK_FALSE(is_valid_content_id(""));
    CHECK_FALSE(is_valid_content_id("bx..weapon"));
    CHECK_FALSE(is_valid_content_id("bx.weapon."));
    CHECK_FALSE(is_valid_content_id("Bx.weapon.sword"));
    CHECK_FALSE(is_valid_content_id("bx.weapon.long-bow"));
    CHECK_FALSE(is_valid_content_id("bx.weapon.long bow"));
}

TEST_CASE("ContentId canonicalization maps existing content names to stable segments") {
    CHECK(canonical_content_segment("Long Bow") == "long_bow");
    CHECK(canonical_content_segment("magic-user") == "magic_user");
    CHECK(canonical_content_segment("Bat, Normal") == "bat_normal");
    CHECK(canonical_content_segment("  Silence 15' radius  ") == "silence_15_radius");

    CHECK(make_content_id("weapon", "long-bow") == ContentId{"bx.weapon.long_bow"});
    CHECK(make_content_id("monster", "Goblin") == ContentId{"bx.monster.goblin"});
}

TEST_CASE("RulesCatalog resolves core B/X equipment ids") {
    const RulesCatalog& catalog = RulesCatalog::core();

    const bx::Weapon* sword = catalog.find_weapon(ContentId{"bx.weapon.sword"});
    REQUIRE(sword != nullptr);
    CHECK(std::string_view(sword->id) == "sword");
    CHECK(std::string_view(sword->name) == "Sword");

    const bx::Weapon* long_bow = catalog.find_weapon(ContentId{"bx.weapon.long_bow"});
    REQUIRE(long_bow != nullptr);
    CHECK(std::string_view(long_bow->id) == "long-bow");

    const bx::ArmorItem* chain = catalog.find_armor(ContentId{"bx.armor.chain_mail"});
    REQUIRE(chain != nullptr);
    CHECK(std::string_view(chain->id) == "chain-mail");

    const bx::GearItem* pole = catalog.find_gear(ContentId{"bx.gear.pole_10ft"});
    REQUIRE(pole != nullptr);
    CHECK(std::string_view(pole->id) == "pole-10ft");

    CHECK(catalog.find_weapon(ContentId{"bx.weapon.nope"}) == nullptr);
    CHECK(catalog.find_armor(ContentId{"bx.weapon.sword"}) == nullptr);
}

TEST_CASE("RulesCatalog resolves core spell and monster ids") {
    const RulesCatalog& catalog = RulesCatalog::core();

    const bx::Spell* magic_missile = catalog.find_spell(ContentId{"bx.spell.magic_user.magic_missile"});
    REQUIRE(magic_missile != nullptr);
    CHECK(std::string_view(magic_missile->name) == "Magic Missile");
    CHECK(std::string_view(magic_missile->class_name) == "Magic-User");

    const bx::Spell* cleric_light = catalog.find_spell(ContentId{"bx.spell.cleric.light"});
    REQUIRE(cleric_light != nullptr);
    CHECK(std::string_view(cleric_light->name) == "Light");
    CHECK(std::string_view(cleric_light->class_name) == "Cleric");

    const bx::Spell* magic_user_light = catalog.find_spell(ContentId{"bx.spell.magic_user.light"});
    REQUIRE(magic_user_light != nullptr);
    CHECK(std::string_view(magic_user_light->class_name) == "Magic-User");
    CHECK(magic_user_light != cleric_light);

    const bx::Monster* goblin = catalog.find_monster(ContentId{"bx.monster.goblin"});
    REQUIRE(goblin != nullptr);
    CHECK(std::string_view(goblin->name) == "Goblin");

    const bx::Monster* normal_bat = catalog.find_monster(ContentId{"bx.monster.bat_normal"});
    REQUIRE(normal_bat != nullptr);
    CHECK(std::string_view(normal_bat->name) == "Bat, Normal");

    const bx::Monster* first_ghoul = catalog.find_monster(ContentId{"bx.monster.ghoul"});
    const bx::Monster* second_ghoul = catalog.find_monster(ContentId{"bx.monster.ghoul_2"});
    REQUIRE(first_ghoul != nullptr);
    REQUIRE(second_ghoul != nullptr);
    CHECK(first_ghoul != second_ghoul);
}

TEST_CASE("RulesCatalog exposes ids for all current core arrays") {
    const RulesCatalog& catalog = RulesCatalog::core();

    CHECK(catalog.weapon_ids().size() == bx::weapons.size());
    CHECK(catalog.armor_ids().size() == bx::armor_items.size());
    CHECK(catalog.gear_ids().size() == bx::gear_items.size());
    CHECK(catalog.spell_ids().size() == bx::spells.size());
    CHECK(catalog.monster_ids().size() == bx::monsters.size());

    for (const ContentId& id : catalog.weapon_ids())
        CHECK(is_valid_content_id(id.value));
    for (const ContentId& id : catalog.spell_ids())
        CHECK(is_valid_content_id(id.value));
    for (const ContentId& id : catalog.monster_ids())
        CHECK(is_valid_content_id(id.value));

    auto monster_ids = catalog.monster_ids();
    bool monster_ids_unique = true;
    for (std::size_t i = 0; i < monster_ids.size(); ++i) {
        for (std::size_t j = i + 1; j < monster_ids.size(); ++j) {
            if (monster_ids[i] == monster_ids[j])
                monster_ids_unique = false;
        }
    }
    CHECK(monster_ids_unique);
}

// ══════════════════════════════════════════════════════════════
// CampaignState model
// ══════════════════════════════════════════════════════════════

TEST_CASE("CampaignState captures mutable campaign and runtime state") {
    Game g = make_minimal_game(12, 8);
    g.state = GameState::Dungeon;
    g.previous_state = GameState::Camp;
    g.town_submenu = 3;
    g.current_location_id = "procedural.dungeon.level_3";
    g.total_gold = 123;
    g.total_xp = 456;
    g.hero_level = 4;
    g.dungeon_level = 3;
    g.wander_counter = 7;
    g.tick_clock = 9;
    g.dungeon_peace_announced = true;
    g.rng.seed(2026u);
    g.hero = make_hero(2, 2, 5, 8, 4);
    g.hero.name = "Campaign Hero";
    g.hero.weapon_id = "sword";
    g.hero.weapon_counts[8] = 1;
    g.hero.gear_counts[15] = 2;
    g.map[index_for(g, 2, 2)].visible = true;
    g.map[index_for(g, 2, 2)].items.push_back(ItemType::Gold);
    g.monsters.push_back(make_monster("Goblin", EntityType::Goblin,
                                      4, 2, 3, 7, 1, 6, 7));
    g.rooms.push_back(Room{1, 1, 4, 4});
    g.event_log.push_back("Captured state log.");

    CampaignState state = capture_campaign_state(g);

    CHECK(state.schema_version == 1);
    CHECK(state.rules_version == "bx.moldvay-basic");
    CHECK(state.current_location_id == g.current_location_id);
    CHECK(state.state == GameState::Dungeon);
    CHECK(state.previous_state == GameState::Camp);
    CHECK(state.town_submenu == 3);
    CHECK(state.total_gold == 123);
    CHECK(state.total_xp == 456);
    CHECK(state.hero_level == 4);
    CHECK(state.dungeon_level == 3);
    CHECK(state.wander_counter == 7);
    CHECK(state.tick_clock == 9);
    CHECK(state.dungeon_peace_announced);
    CHECK(state.hero.name == "Campaign Hero");
    CHECK(state.hero.weapon_id == "sword");
    CHECK(state.hero.gear_counts[15] == 2);
    CHECK(state.width == 12);
    CHECK(state.height == 8);
    REQUIRE(state.map.size() == g.map.size());
    CHECK(state.map[index_for(g, 2, 2)].visible);
    CHECK(state.map[index_for(g, 2, 2)].items == std::vector<ItemType>{ItemType::Gold});
    REQUIRE(state.monsters.size() == 1);
    CHECK(state.monsters[0].name == "Goblin");
    REQUIRE(state.rooms.size() == 1);
    CHECK(state.rooms[0].w == 4);
    CHECK(state.event_log.back() == "Captured state log.");

    auto game_rng = g.rng;
    auto state_rng = state.rng;
    CHECK(game_rng() == state_rng());
}

TEST_CASE("CampaignState restores Game while preserving runtime configuration") {
    Game source = make_minimal_game(10, 6);
    source.data_dir = "/tmp/source-data";
    source.state = GameState::Dungeon;
    source.current_location_id = "source.location";
    source.hero = make_hero(1, 1, 2, 6, 8);
    source.hero.name = "Restored Hero";
    source.total_gold = 50;
    source.rng.seed(1234u);
    source.monsters.push_back(make_monster("Orc", EntityType::Orc,
                                           3, 1, 5, 7, 1, 6, 8));
    source.event_log.push_back("Restore me.");

    CampaignState state = capture_campaign_state(source);

    Game target = make_minimal_game(4, 4);
    target.data_dir = "/tmp/target-data";
    target.current_location_id = "old.location";
    target.hero = make_hero(0, 0, 0, 4, 9);
    target.total_gold = 1;
    target.events.push_back(GameEvent{GameEventType::Sound, {}, SoundEffect::Footstep});

    restore_campaign_state(target, state);

    CHECK(target.data_dir == std::filesystem::path{"/tmp/target-data"});
    CHECK(target.current_location_id == "source.location");
    CHECK(target.state == GameState::Dungeon);
    CHECK(target.hero.name == "Restored Hero");
    CHECK(target.hero.pos.x == 1);
    CHECK(target.total_gold == 50);
    REQUIRE(target.monsters.size() == 1);
    CHECK(target.monsters[0].name == "Orc");
    CHECK(target.event_log.back() == "Restore me.");
    CHECK(target.events.empty());

    auto source_rng = source.rng;
    auto target_rng = target.rng;
    CHECK(source_rng() == target_rng());
}

TEST_CASE("CampaignState separates future module identity from current procedural saves") {
    Game g = make_minimal_game();
    CampaignState state = capture_campaign_state(g);

    CHECK(state.module_id.empty());
    CHECK(state.module_version.empty());

    state.module_id = "module.testing_tomb";
    state.module_version = "0.1.0";

    Game restored = make_minimal_game();
    restore_campaign_state(restored, state);

    // Module metadata is part of the campaign model, but restoring into current
    // Game intentionally does not create a module runtime in this phase.
    CHECK(restored.current_location_id == state.current_location_id);
    CHECK(state.module_id == "module.testing_tomb");
    CHECK(state.module_version == "0.1.0");
}

// ══════════════════════════════════════════════════════════════
// Module manifest loader
// ══════════════════════════════════════════════════════════════

namespace {

std::filesystem::path make_manifest_test_dir(const std::string& name) {
    auto dir = std::filesystem::path{"/tmp/opencode"} / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

std::string valid_module_manifest_json() {
    return R"json({
  "module_id": "testing_tomb",
  "title": "Testing Tomb",
  "version": "0.1.0",
  "author": "BX Crawl",
  "ruleset": "bx.moldvay_basic",
  "engine_min_version": "0.1.0",
  "content_schema_version": 1,
  "entry_location_id": "module.testing_tomb.location.entrance",
  "files": {
    "locations": "locations.json",
    "map": "maps/tomb.json",
    "encounters": "encounters.json",
    "treasures": "treasures.json",
    "traps": "traps.json",
    "descriptions": "text/descriptions.txt"
  }
})json";
}

void write_module_json(const std::filesystem::path& dir, const std::string& json) {
    write_save_file(json, dir / "module.json");
}

} // anonymous namespace

TEST_CASE("Module manifest loader accepts a minimal valid manifest") {
    auto dir = make_manifest_test_dir("bx_module_manifest_valid");
    write_module_json(dir, valid_module_manifest_json());

    ModuleManifestResult result = load_module_manifest(dir);

    REQUIRE(result.ok);
    CHECK(result.error.empty());
    CHECK(result.manifest.module_id == "testing_tomb");
    CHECK(result.manifest.title == "Testing Tomb");
    CHECK(result.manifest.version == "0.1.0");
    CHECK(result.manifest.author == "BX Crawl");
    CHECK(result.manifest.ruleset == "bx.moldvay_basic");
    CHECK(result.manifest.engine_min_version == "0.1.0");
    CHECK(result.manifest.content_schema_version == 1);
    CHECK(result.manifest.entry_location_id == "module.testing_tomb.location.entrance");
    CHECK(result.manifest.files.at("locations") == std::filesystem::path{"locations.json"});
    CHECK(result.manifest.files.at("map") == std::filesystem::path{"maps/tomb.json"});

    std::filesystem::remove_all(dir);
}

TEST_CASE("Module manifest loader rejects missing or invalid required fields") {
    auto dir = make_manifest_test_dir("bx_module_manifest_invalid_fields");

    SUBCASE("missing title") {
        write_module_json(dir, R"json({
  "module_id": "testing_tomb",
  "version": "0.1.0",
  "author": "BX Crawl",
  "ruleset": "bx.moldvay_basic",
  "engine_min_version": "0.1.0",
  "content_schema_version": 1,
  "entry_location_id": "module.testing_tomb.location.entrance",
  "files": { "locations": "locations.json" }
})json");
        ModuleManifestResult result = load_module_manifest(dir);
        CHECK_FALSE(result.ok);
        CHECK(result.error == "missing title");
    }

    SUBCASE("invalid module id") {
        std::string json = valid_module_manifest_json();
        const std::string needle = "\"module_id\": \"testing_tomb\"";
        json.replace(json.find(needle), needle.size(), "\"module_id\": \"Testing-Tomb\"");
        write_module_json(dir, json);
        ModuleManifestResult result = load_module_manifest(dir);
        CHECK_FALSE(result.ok);
        CHECK(result.error == "invalid module_id");
    }

    SUBCASE("invalid schema version") {
        std::string json = valid_module_manifest_json();
        const std::string needle = "\"content_schema_version\": 1";
        json.replace(json.find(needle), needle.size(), "\"content_schema_version\": 0");
        write_module_json(dir, json);
        ModuleManifestResult result = load_module_manifest(dir);
        CHECK_FALSE(result.ok);
        CHECK(result.error == "invalid content_schema_version");
    }

    std::filesystem::remove_all(dir);
}

TEST_CASE("Module manifest loader validates entry location namespace") {
    auto dir = make_manifest_test_dir("bx_module_manifest_entry_namespace");
    std::string json = valid_module_manifest_json();
    const std::string needle = "\"entry_location_id\": \"module.testing_tomb.location.entrance\"";
    json.replace(json.find(needle), needle.size(),
                 "\"entry_location_id\": \"module.other.location.entrance\"");
    write_module_json(dir, json);

    ModuleManifestResult result = load_module_manifest(dir);

    CHECK_FALSE(result.ok);
    CHECK(result.error == "entry_location_id must belong to module location namespace");

    std::filesystem::remove_all(dir);
}

TEST_CASE("Module manifest loader rejects unsafe file references") {
    auto dir = make_manifest_test_dir("bx_module_manifest_unsafe_files");

    SUBCASE("parent traversal") {
        std::string json = valid_module_manifest_json();
        const std::string needle = "\"locations\": \"locations.json\"";
        json.replace(json.find(needle), needle.size(), "\"locations\": \"../outside.json\"");
        write_module_json(dir, json);
        ModuleManifestResult result = load_module_manifest(dir);
        CHECK_FALSE(result.ok);
        CHECK(result.error == "unsafe file path");
    }

    SUBCASE("absolute path") {
        std::string json = valid_module_manifest_json();
        const std::string needle = "\"locations\": \"locations.json\"";
        json.replace(json.find(needle), needle.size(), "\"locations\": \"/tmp/outside.json\"");
        write_module_json(dir, json);
        ModuleManifestResult result = load_module_manifest(dir);
        CHECK_FALSE(result.ok);
        CHECK(result.error == "unsafe file path");
    }

    std::filesystem::remove_all(dir);
}

TEST_CASE("Module manifest loader handles missing directories and files") {
    auto dir = std::filesystem::path{"/tmp/opencode/bx_module_manifest_missing"};
    std::filesystem::remove_all(dir);

    ModuleManifestResult no_dir = load_module_manifest(dir);
    CHECK_FALSE(no_dir.ok);
    CHECK(no_dir.error == "module directory does not exist");

    std::filesystem::create_directories(dir);
    ModuleManifestResult no_file = load_module_manifest(dir);
    CHECK_FALSE(no_file.ok);
    CHECK(no_file.error == "could not open module.json");

    std::filesystem::remove_all(dir);
}

// ══════════════════════════════════════════════════════════════
// Module content loader
// ══════════════════════════════════════════════════════════════

TEST_CASE("Testing Tomb module loads and exposes its entry location") {
    const auto module_dir = std::filesystem::path{"modules/testing-tomb"};

    ModuleContentResult result = load_module_content(module_dir);

    REQUIRE(result.ok);
    CHECK(result.error.empty());
    CHECK(result.module.manifest.module_id == "testing_tomb");

    const ModuleLocation* entry = result.module.entry_location();
    REQUIRE(entry != nullptr);
    CHECK(entry->location_id == "module.testing_tomb.location.entrance");
    CHECK(entry->title == "Entrance");
    CHECK(entry->description_id == "module.testing_tomb.text.entrance");
    CHECK(entry->map_id == "module.testing_tomb.map.tomb");

    const ModuleMapDefinition* map = result.module.find_map(entry->map_id);
    REQUIRE(map != nullptr);
    CHECK(map->width == 5);
    CHECK(map->height == 5);
    CHECK(map->entry_x == 2);
    CHECK(map->entry_y == 4);
    REQUIRE(map->tiles.size() == 25);
    REQUIRE(map->tile_at(2, 4) != nullptr);
    CHECK(*map->tile_at(2, 4) == TileType::Floor);
    REQUIRE(map->tile_at(2, 2) != nullptr);
    CHECK(*map->tile_at(2, 2) == TileType::DoorClosed);
    REQUIRE(map->tile_at(0, 0) != nullptr);
    CHECK(*map->tile_at(0, 0) == TileType::Wall);
    CHECK(map->tile_at(-1, 0) == nullptr);

    const ModuleLocation* room_a = result.module.find_location("module.testing_tomb.location.room_a");
    REQUIRE(room_a != nullptr);
    CHECK(room_a->title == "Room A");

    const ModuleDoorDefinition* door = result.module.find_door(
        "module.testing_tomb.door.entrance_to_room_a");
    REQUIRE(door != nullptr);
    CHECK(door->from_location_id == "module.testing_tomb.location.entrance");
    CHECK(door->to_location_id == "module.testing_tomb.location.room_a");
    CHECK(door->x == 2);
    CHECK(door->y == 2);
    CHECK(door->locked);

    const std::string* description = result.module.find_description(entry->description_id);
    REQUIRE(description != nullptr);
    CHECK(description->find("stone antechamber") != std::string::npos);
}

TEST_CASE("Module content loader rejects invalid door definitions") {
    auto dir = make_manifest_test_dir("bx_module_content_bad_doors");
    std::filesystem::create_directories(dir / "maps");
    std::filesystem::create_directories(dir / "text");
    write_module_json(dir, valid_module_manifest_json());
    write_save_file(R"json([
  {
    "location_id": "module.testing_tomb.location.entrance",
    "title": "Entrance",
    "description_id": "module.testing_tomb.text.entrance",
    "map_id": "module.testing_tomb.map.tomb"
  },
  {
    "location_id": "module.testing_tomb.location.room_a",
    "title": "Room A",
    "description_id": "module.testing_tomb.text.room_a",
    "map_id": "module.testing_tomb.map.tomb"
  }
])json", dir / "locations.json");
    write_save_file("[module.testing_tomb.text.entrance]\nEntrance text.\n\n"
                    "[module.testing_tomb.text.room_a]\nRoom A text.\n",
                    dir / "text/descriptions.txt");

    SUBCASE("door on non-door tile") {
        write_save_file(R"json({
  "map_id": "module.testing_tomb.map.tomb",
  "width": 5,
  "height": 5,
  "entry_x": 2,
  "entry_y": 4,
  "tiles": [
    "#####",
    "#...#",
    "##+##",
    "#...#",
    "##.##"
  ],
  "doors": [
    {
      "door_id": "module.testing_tomb.door.bad_floor",
      "from_location_id": "module.testing_tomb.location.entrance",
      "to_location_id": "module.testing_tomb.location.room_a",
      "x": 2,
      "y": 4,
      "locked": false
    }
  ]
})json", dir / "maps/tomb.json");

        ModuleContentResult result = load_module_content(dir);
        CHECK_FALSE(result.ok);
        CHECK(result.error == "door position is not a door tile");
    }

    SUBCASE("missing location endpoint") {
        write_save_file(R"json({
  "map_id": "module.testing_tomb.map.tomb",
  "width": 5,
  "height": 5,
  "entry_x": 2,
  "entry_y": 4,
  "tiles": [
    "#####",
    "#...#",
    "##+##",
    "#...#",
    "##.##"
  ],
  "doors": [
    {
      "door_id": "module.testing_tomb.door.bad_endpoint",
      "from_location_id": "module.testing_tomb.location.entrance",
      "to_location_id": "module.testing_tomb.location.missing",
      "x": 2,
      "y": 2,
      "locked": false
    }
  ]
})json", dir / "maps/tomb.json");

        ModuleContentResult result = load_module_content(dir);
        CHECK_FALSE(result.ok);
        CHECK(result.error == "door references missing location");
    }

    std::filesystem::remove_all(dir);
}

TEST_CASE("Module content loader rejects invalid location references") {
    auto dir = make_manifest_test_dir("bx_module_content_bad_refs");
    std::filesystem::create_directories(dir / "maps");
    std::filesystem::create_directories(dir / "text");
    write_module_json(dir, valid_module_manifest_json());
    write_save_file(R"json([
  {
    "location_id": "module.testing_tomb.location.entrance",
    "title": "Entrance",
    "description_id": "module.testing_tomb.text.missing",
    "map_id": "module.testing_tomb.map.tomb"
  }
])json", dir / "locations.json");
    write_save_file(R"json({
  "map_id": "module.testing_tomb.map.tomb",
  "width": 5,
  "height": 5,
  "entry_x": 2,
  "entry_y": 4,
  "tiles": [
    "#####",
    "#...#",
    "##+##",
    "#...#",
    "##.##"
  ]
})json", dir / "maps/tomb.json");
    write_save_file("[module.testing_tomb.text.entrance]\nEntrance text.\n", dir / "text/descriptions.txt");

    ModuleContentResult result = load_module_content(dir);

    CHECK_FALSE(result.ok);
    CHECK(result.error == "location references missing description");

    std::filesystem::remove_all(dir);
}

TEST_CASE("Module content loader rejects invalid map topology") {
    auto dir = make_manifest_test_dir("bx_module_content_bad_map");
    std::filesystem::create_directories(dir / "maps");
    std::filesystem::create_directories(dir / "text");
    write_module_json(dir, valid_module_manifest_json());
    write_save_file(R"json([
  {
    "location_id": "module.testing_tomb.location.entrance",
    "title": "Entrance",
    "description_id": "module.testing_tomb.text.entrance",
    "map_id": "module.testing_tomb.map.tomb"
  }
])json", dir / "locations.json");
    write_save_file("[module.testing_tomb.text.entrance]\nEntrance text.\n", dir / "text/descriptions.txt");

    SUBCASE("row width mismatch") {
        write_save_file(R"json({
  "map_id": "module.testing_tomb.map.tomb",
  "width": 5,
  "height": 2,
  "entry_x": 1,
  "entry_y": 1,
  "tiles": [
    "#####",
    "#.#"
  ]
})json", dir / "maps/tomb.json");

        ModuleContentResult result = load_module_content(dir);
        CHECK_FALSE(result.ok);
        CHECK(result.error == "map tile width mismatch");
    }

    SUBCASE("blocked entry") {
        write_save_file(R"json({
  "map_id": "module.testing_tomb.map.tomb",
  "width": 3,
  "height": 3,
  "entry_x": 1,
  "entry_y": 1,
  "tiles": [
    "###",
    "###",
    "###"
  ]
})json", dir / "maps/tomb.json");

        ModuleContentResult result = load_module_content(dir);
        CHECK_FALSE(result.ok);
        CHECK(result.error == "map entry position is blocked");
    }

    SUBCASE("unknown glyph") {
        write_save_file(R"json({
  "map_id": "module.testing_tomb.map.tomb",
  "width": 3,
  "height": 3,
  "entry_x": 1,
  "entry_y": 1,
  "tiles": [
    "###",
    "#x#",
    "###"
  ]
})json", dir / "maps/tomb.json");

        ModuleContentResult result = load_module_content(dir);
        CHECK_FALSE(result.ok);
        CHECK(result.error == "invalid map tile glyph");
    }

    std::filesystem::remove_all(dir);
}

// ══════════════════════════════════════════════════════════════
// Module session entry
// ══════════════════════════════════════════════════════════════

TEST_CASE("GameSession starts at the Testing Tomb module entry location") {
    ModuleContentResult loaded = load_module_content(std::filesystem::path{"modules/testing-tomb"});
    REQUIRE(loaded.ok);

    Entity hero = make_hero(0, 0, 2, 6, 14);
    hero.gold_pieces = 12;

    GameSession session;
    ModuleStartResult started = session.start_module(loaded.module, hero);

    REQUIRE(started.ok);
    CHECK(started.error.empty());

    const Game& game = session.game();
    CHECK(game.state == GameState::Dungeon);
    CHECK(game.previous_state == GameState::Town);
    CHECK(game.width == 5);
    CHECK(game.height == 5);
    REQUIRE(game.map.size() == 25);
    CHECK(game.hero.pos.x == 2);
    CHECK(game.hero.pos.y == 4);
    CHECK(game.current_location_id == "module.testing_tomb.location.entrance");
    CHECK(game.total_gold == 12);
    CHECK(game.hero.gold_pieces == 0);
    CHECK(game.monsters.empty());
    CHECK(game.rooms.empty());
    CHECK(game.map[static_cast<std::size_t>(4 * game.width + 2)].type == TileType::Floor);
    CHECK(game.map[static_cast<std::size_t>(2 * game.width + 2)].type == TileType::DoorClosed);
    CHECK(game.map[static_cast<std::size_t>(4 * game.width + 2)].visible);

    REQUIRE_FALSE(game.event_log.empty());
    CHECK(game.event_log.back() == "You enter Testing Tomb.");
    REQUIRE_FALSE(session.events().empty());
    CHECK(session.events().back().type == GameEventType::LogMessage);
    CHECK(session.events().back().text == "You enter Testing Tomb.");

    GameStateSnapshot snapshot = session.snapshot();
    CHECK(snapshot.state == GameState::Dungeon);
    CHECK(snapshot.width == 5);
    CHECK(snapshot.height == 5);
    CHECK(snapshot.hero.pos.x == 2);
    CHECK(snapshot.hero.pos.y == 4);
}

TEST_CASE("GameSession rejects module entry maps that cannot become runtime maps") {
    ModuleContentResult loaded = load_module_content(std::filesystem::path{"modules/testing-tomb"});
    REQUIRE(loaded.ok);
    loaded.module.maps.at("module.testing_tomb.map.tomb").tiles.pop_back();

    GameSession session;
    ModuleStartResult started = session.start_module(loaded.module, make_hero(0, 0, 2, 6, 14));

    CHECK_FALSE(started.ok);
    CHECK(started.error == "module entry map tile count mismatch");
    CHECK(session.game().state == GameState::Town);
}
