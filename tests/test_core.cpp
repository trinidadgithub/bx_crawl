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

