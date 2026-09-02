#include "game.hpp"

#include <sstream>
#include <string>
#include <string_view>

namespace {

void log_event(Game& g, const std::string& msg) {
    g.event_log.push_back(msg);
    g.events.push_back(GameEvent{GameEventType::LogMessage, msg});
    const std::size_t max_events = 8;
    if (g.event_log.size() > max_events) {
        g.event_log.erase(g.event_log.begin(),
                          g.event_log.begin() + (g.event_log.size() - max_events));
    }
}

} // namespace

SurpriseResult resolve_surprise(int hero_roll, int monster_roll,
                                int hero_threshold,
                                int monster_threshold) {
    return SurpriseResult{
        hero_roll,
        monster_roll,
        hero_roll <= hero_threshold,
        monster_roll <= monster_threshold
    };
}

SurpriseResult roll_surprise(Game& g, int hero_threshold, int monster_threshold) {
    return resolve_surprise(roll_d(g, 6), roll_d(g, 6),
                            hero_threshold, monster_threshold);
}

int monster_surprise_threshold(std::string_view name) {
    // B/X stealthy monsters surprise opponents on better odds than 1-2.
    if (name.rfind("Bugbear", 0) == 0)                return 3;  // p. B32
    if (name.rfind("Spider, Crab", 0) == 0)           return 4;  // p. B43
    if (name.rfind("Shadow", 0) == 0)                 return 5;  // p. B42
    if (name.rfind("Lycanthrope, Weretiger", 0) == 0) return 4;  // B/X lycanthrope
    return 2;
}

bool roll_initiative(Game& g) {
    int hero_roll = roll_d(g, 6);
    int monster_roll = roll_d(g, 6);

    // Re-roll ties.
    while (hero_roll == monster_roll) {
        hero_roll = roll_d(g, 6);
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

bool any_monsters_alive(const Game& g) {
    for (const auto& m : g.monsters) {
        if (m.alive) return true;
    }
    return false;
}

void announce_dungeon_at_peace(Game& g) {
    if (g.dungeon_peace_announced) return;
    g.dungeon_peace_announced = true;
    log_event(g, "This place is now at peace, thanks to your valiant effort.");
    log_event(g, "You may keep exploring for hidden treasure and supplies.");
}
