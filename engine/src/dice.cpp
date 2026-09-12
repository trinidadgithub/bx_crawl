#include "dice.hpp"

int DiceRoller::d(int sides) {
    std::uniform_int_distribution<int> dist(1, sides);
    return dist(engine_);
}

int DiceRoller::range(int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(engine_);
}