#include "game.hpp"
#include "game_internal.hpp"
#include "map.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <string_view>

namespace {

void log_event(Game& g, const std::string& msg) {
    game_internal::emit_log_event(g, msg);
}

void queue_sound(Game& g, SoundEffect effect) {
    game_internal::emit_sound(g, effect);
}

bool hero_is_confused(const Game& g) {
    return game_internal::hero_is_confused(g);
}

int daylight_to_hit_penalty(const Game& g, const Entity& m) {
    return game_internal::daylight_to_hit_penalty(g, m);
}

int effective_morale(const Game& g, const Entity& m) {
    return game_internal::effective_morale(g, m);
}

bool make_save(Game& g, CharacterClass cc, int category) {
    return game_internal::make_save(g, cc, category);
}

void award_treasure(Game& g, Entity& monster) {
    game_internal::award_treasure(g, monster);
}

void check_level_up(Game& g) {
    game_internal::check_level_up(g);
}

void apply_level_drain(Game& g, const Entity& draining) {
    game_internal::apply_level_drain(g, draining);
}

void apply_strength_drain(Game& g, const Entity& draining) {
    game_internal::apply_strength_drain(g, draining);
}

void apply_rust_corrosion(Game& g, const Entity& rusting) {
    game_internal::apply_rust_corrosion(g, rusting);
}

void apply_green_slime(Game& g, const Entity& slime) {
    game_internal::apply_green_slime(g, slime);
}

bool hero_weapon_can_wound(const Game& g, WeaponImmunity immunity) {
    return game_internal::hero_weapon_can_wound(g, immunity);
}

int sign(int v) {
    return game_internal::sign(v);
}

bool is_occupied(const Game& g, int x, int y, const Entity* ignore = nullptr) {
    return game_internal::is_occupied(g, x, y, ignore);
}

void move_entity(Game& g, Entity& e, int dx, int dy) {
    game_internal::move_entity(g, e, dx, dy);
}

int roll_2d6(Game& g) {
    return roll_d(g, 6) + roll_d(g, 6);
}

struct BearAttackProfile {
    int claw_die{};
    int bite_die{};
};

bool bear_attack_profile(std::string_view name, BearAttackProfile& profile) {
    if (name.rfind("Bear,", 0) != 0) return false;

    if (name.find("Black") != std::string_view::npos) {
        profile = {3, 6};
    } else if (name.find("Cave") != std::string_view::npos) {
        profile = {8, 12};
    } else if (name.find("Grizzly") != std::string_view::npos) {
        profile = {4, 8};
    } else if (name.find("Polar") != std::string_view::npos) {
        profile = {6, 10};
    } else {
        return false;
    }

    return true;
}

void attack_internal(Game& g, Entity& attacker, Entity& defender,
                     int bonus_to_hit, bool trigger_morale);

bool resolve_bear_attack(Game& g, Entity& attacker, Entity& defender,
                         int bonus_to_hit) {
    BearAttackProfile profile{};
    if (attacker.type == EntityType::Hero || defender.type != EntityType::Hero
        || !bear_attack_profile(attacker.name, profile)) {
        return false;
    }

    int target = defender.ac;
    if (defender.shield_turns > 0)         target += 4;
    if (defender.protection_turns > 0)     target += 1;
    if (defender.resist_cold_turns > 0)    target += 1;

    bool claw_hits[2] = {false, false};

    auto resolve_one = [&](const char* attack_name, int damage_die, int claw_index) {
        if (!attacker.alive || !defender.alive) return;

        int roll = roll_d(g, 20);
        int total = roll + attacker.attack_bonus + bonus_to_hit;
        {
            std::ostringstream oss;
            oss << attacker.name << " " << attack_name << " attacks " << defender.name
                << " (d20 = " << roll << ", total " << total
                << " vs AC " << target << ")";
            log_event(g, oss.str());
        }

        if (total < target) {
            log_event(g, "Miss!");
            return;
        }

        int dmg = std::max(1, roll_d(g, damage_die));
        defender.hp -= dmg;
        if (claw_index >= 0) claw_hits[claw_index] = true;

        {
            std::ostringstream oss;
            oss << "Hit! " << defender.name << " takes " << dmg << " damage.";
            log_event(g, oss.str());
        }

        if (defender.hp <= 0) {
            defender.hp = 0;
            defender.alive = false;
            queue_sound(g, SoundEffect::PlayerDeath);
            log_event(g, defender.name + " is slain!");
        }
    };

    resolve_one("claw", profile.claw_die, 0);
    resolve_one("claw", profile.claw_die, 1);
    resolve_one("bite", profile.bite_die, -1);

    if (defender.alive && claw_hits[0] && claw_hits[1]) {
        int hug_dmg = roll_d(g, 8) + roll_d(g, 8);
        defender.hp -= hug_dmg;
        {
            std::ostringstream oss;
            oss << attacker.name << " hugs " << defender.name
                << " for " << hug_dmg << " additional damage!";
            log_event(g, oss.str());
        }
        if (defender.hp <= 0) {
            defender.hp = 0;
            defender.alive = false;
            queue_sound(g, SoundEffect::PlayerDeath);
            log_event(g, defender.name + " is slain!");
        }
    }

    return true;
}

void morale_allies_on_death(Game& g, const Entity& fallen, Entity& attacker,
                            int& free_attacks_remaining);

void attack_internal(Game& g, Entity& attacker, Entity& defender,
                     int bonus_to_hit, bool trigger_morale) {
    if (!attacker.alive || !defender.alive) return;

    if (resolve_bear_attack(g, attacker, defender, bonus_to_hit)) return;

    int roll = roll_d(g, 20);
    int extra_atk = bonus_to_hit;
    if (attacker.type == EntityType::Hero && attacker.courage_turns > 0)
        extra_atk += 1;
    if (attacker.type == EntityType::Hero && hero_is_confused(g))
        extra_atk -= 2;
    if (attacker.type == EntityType::Hero) {
        if (attacker.turns_without_food >= 144)
            extra_atk -= 2;
        else if (attacker.turns_without_food >= 96)
            extra_atk -= 1;
    }
    extra_atk -= daylight_to_hit_penalty(g, attacker);
    int backstab_bonus = (attacker.type == EntityType::Hero && attacker.hidden) ? 4 : 0;
    int total = roll + attacker.attack_bonus + extra_atk + backstab_bonus;
    int target = defender.ac;

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
        bool weapon_can_wound = true;
        if (attacker.type == EntityType::Hero
            && defender.weapon_immunity != WeaponImmunity::None) {
            weapon_can_wound = hero_weapon_can_wound(g, defender.weapon_immunity);
        }

        if (!weapon_can_wound) {
            {
                std::ostringstream oss;
                if (defender.weapon_immunity == WeaponImmunity::FireOnly) {
                    oss << "Your weapon passes right through the "
                        << defender.name << "!";
                } else {
                    oss << "The " << defender.name << " is unharmed by your weapon!";
                }
                log_event(g, oss.str());
            }
        } else {
            int dmg = roll_d(g, attacker.damage_die) + attacker.damage_bonus;
            dmg = std::max(1, dmg);
            if (backstab_bonus) {
                dmg *= 2;
                queue_sound(g, SoundEffect::Backstab);
                log_event(g, "Backstab hits for double damage!");
            } else if (attacker.type == EntityType::Hero) {
                queue_sound(g, SoundEffect::AttackHit);
            }
            defender.hp -= dmg;

            {
                std::ostringstream oss;
                oss << "Hit! " << defender.name << " takes "
                    << dmg << " damage.";
                log_event(g, oss.str());
            }

            if (defender.hp <= 0) {
                defender.hp = 0;
                defender.alive = false;

                if (defender.type == EntityType::Hero) {
                    queue_sound(g, SoundEffect::PlayerDeath);
                } else {
                    queue_sound(g, SoundEffect::MonsterDeath);
                }

                {
                    std::ostringstream oss;
                    oss << defender.name << " is slain!";
                    log_event(g, oss.str());
                }

                if (defender.type != EntityType::Hero && defender.is_leader) {
                    std::ostringstream oss;
                    oss << "The " << defender.name
                        << " leader has fallen — the group's morale wavers!";
                    log_event(g, oss.str());
                }

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
                if (defender.type == EntityType::Hero && attacker.type != EntityType::Hero) {
                    if (attacker.has_poison) {
                        if (make_save(g, g.hero.char_class, 0)) {
                            log_event(g, "You resist the poison!");
                        } else {
                            int poison_dmg = roll_d(g, 4);
                            defender.hp -= poison_dmg;
                            {
                                std::ostringstream oss;
                                oss << attacker.name << " poison deals " << poison_dmg << " damage!";
                                log_event(g, oss.str());
                            }
                            if (defender.hp <= 0) {
                                defender.hp = 0;
                                defender.alive = false;
                                log_event(g, "You succumb to the poison!");
                            }
                        }
                    } else if (attacker.has_disease) {
                        if (!make_save(g, g.hero.char_class, 0)) {
                            log_event(g, "The " + attacker.name + " infects you with disease!");
                            int disease_dmg = 1;
                            defender.hp -= disease_dmg;
                            if (defender.hp <= 0) {
                                defender.hp = 0;
                                defender.alive = false;
                            }
                        }
                    } else if (attacker.has_paralysis) {
                        bool elf_immune = (g.hero.char_class == CharacterClass::Elf
                                           && attacker.name.find("Ghoul") != std::string::npos);
                        if (elf_immune) {
                            log_event(g, "Your elven blood resists the ghoul's paralysis!");
                        } else if (make_save(g, g.hero.char_class, 2)) {
                            log_event(g, "You resist the paralysis!");
                        } else {
                            g.hero.paralyzed_turns = roll_d(g, 4) + roll_d(g, 4);
                            {
                                std::ostringstream oss;
                                oss << attacker.name << " paralyzes you for "
                                    << g.hero.paralyzed_turns << " turns!";
                                log_event(g, oss.str());
                            }
                        }
                    } else if (attacker.is_vampire_bat) {
                        if (make_save(g, g.hero.char_class, 2)) {
                            log_event(g, "You shake off the vampire bat's bite!");
                        } else {
                            g.hero.unconscious_turns = roll_d(g, 10);
                            {
                                std::ostringstream oss;
                                oss << "The vampire bat's bite knocks you unconscious for "
                                    << g.hero.unconscious_turns << " rounds!";
                                log_event(g, oss.str());
                            }
                        }
                    } else if (attacker.drains_level) {
                        apply_level_drain(g, attacker);
                    } else if (attacker.drains_strength) {
                        apply_strength_drain(g, attacker);
                    } else if (attacker.corrodes_metal) {
                        apply_rust_corrosion(g, attacker);
                    } else if (attacker.green_slime) {
                        apply_green_slime(g, attacker);
                    }
                }
                if (trigger_morale && defender_is_monster &&
                    attacker.type == EntityType::Hero) {
                    try_morale_check(g, defender, attacker,
                                     free_attacks_remaining, "is wounded");
                }
            }
        }
    } else {
        log_event(g, "Miss!");
    }
}

void morale_allies_on_death(Game& g, const Entity& fallen, Entity& attacker,
                            int& free_attacks_remaining) {
    for (auto& m : g.monsters) {
        if (!m.alive) continue;
        if (m.type != fallen.type) continue;
        if (m.morale <= 0) continue;

        try_morale_check(g, m, attacker, free_attacks_remaining,
                         "an ally dies");
    }
}

} // namespace

void try_morale_check(Game& g, Entity& m, Entity& attacker,
                      int& free_attacks_remaining, const std::string& reason) {
    if (!m.alive) return;
    if (m.type == EntityType::Hero) return;
    int morale = effective_morale(g, m);
    if (m.morale <= 0 && morale <= 0) return;

    int roll = roll_2d6(g);

    {
        std::ostringstream oss;
        oss << m.name << " makes a morale check (" << reason
            << "): 2d6 = " << roll << " vs morale " << morale;
        log_event(g, oss.str());
    }

    if (roll > morale) {
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

    if (std::abs(dx) >= std::abs(dy)) {
        if (!try_step(step_x, 0)) {
            (void)try_step(0, step_y);
        }
    } else {
        if (!try_step(0, step_y)) {
            (void)try_step(step_x, 0);
        }
    }

    if (m.pos.x == 0 || m.pos.x == g.width - 1 ||
        m.pos.y == 0 || m.pos.y == g.height - 1) {
        m.alive = false;
        std::ostringstream oss;
        oss << m.name << " flees into the darkness!";
        log_event(g, oss.str());
    }
}

void attack(Game& g, Entity& attacker, Entity& defender) {
    if (attacker.type == EntityType::Hero && defender.charmed) {
        defender.charmed = false;
        log_event(g, defender.name + " breaks free of the charm!");
    }

    attack_internal(g, attacker, defender,
                    /*bonus_to_hit=*/0,
                    /*trigger_morale=*/true);
}

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
