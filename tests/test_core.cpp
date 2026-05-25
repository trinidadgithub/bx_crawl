#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "third_party/doctest.h"

#include "game.hpp"
#include "map.hpp"

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

    SUBCASE("all room tiles are floor") {
        auto rooms = init_map(g);
        for (const auto& r : rooms) {
            for (int y = r.y; y < r.y + r.h; ++y) {
                for (int x = r.x; x < r.x + r.w; ++x) {
                    CHECK(g.map[index_for(g, x, y)].type == TileType::Floor);
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
        bool result = roll_initiative(g);

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
    return e;
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

