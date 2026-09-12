#pragma once

#include <random>

// DiceRoller is the engine's RandomService: each game owns one, and every
// random draw funnels through `d()` (1..sides) or `range()` (lo..hi). Gameplay
// code should use the rule-level helpers (e.g. roll_d) rather than touching the
// wrapped engine directly, so deterministic testing and future replay /
// roll-history support have a single place to hook.
//
// The class also keeps the std::mt19937 surface (seed / operator() / result_type
// / min / max), so call sites that seed, copy, or compare RNG state keep working
// unchanged: the underlying engine lives in the wrapped `std::mt19937`.
class DiceRoller {
public:
    using result_type = std::mt19937::result_type;

    static constexpr result_type min() { return std::mt19937::min(); }
    static constexpr result_type max() { return std::mt19937::max(); }

    DiceRoller() = default;
    DiceRoller(const DiceRoller&) = default;
    DiceRoller& operator=(const DiceRoller&) = default;

    void seed(result_type value) { engine_.seed(value); }
    result_type operator()() { return engine_(); }

    // Uniform draw in [1, sides].
    int d(int sides);
    // Uniform draw in [lo, hi], inclusive.
    int range(int lo, int hi);

private:
    std::mt19937 engine_;
};