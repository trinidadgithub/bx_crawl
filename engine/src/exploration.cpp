#include "game.hpp"
#include "game_internal.hpp"
#include "bx_data.hpp"
#include "item_effects.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string>

namespace {

void log_event(Game& g, const std::string& msg) {
    game_internal::emit_log_event(g, msg);
}

void queue_sound(Game& g, SoundEffect effect) {
    game_internal::emit_sound(g, effect);
}

bool make_save(Game& g, CharacterClass cc, int category) {
    return game_internal::make_save(g, cc, category);
}

} // namespace

int carried_weight_cn(const Game& g) {
    int weight = g.total_gold; // B/X coin-weight: treasure coins count directly.

    for (std::size_t i = 0; i < bx::weapons.size(); ++i)
        weight += g.hero.weapon_counts[i] * bx::weapons[i].weight_cn;
    for (std::size_t i = 0; i < bx::armor_items.size(); ++i)
        weight += g.hero.armor_counts[i] * bx::armor_items[i].weight_cn;
    for (std::size_t i = 0; i < bx::gear_items.size(); ++i)
        weight += g.hero.gear_counts[i] * bx::gear_items[i].weight_cn;

    return weight;
}

int exploration_move_feet(const Game& g) {
    int weight = carried_weight_cn(g);
    if (weight <= 400) return 120;
    if (weight <= 600) return 90;
    if (weight <= 800) return 60;
    return 30;
}

int encounter_move_feet(const Game& g) {
    return exploration_move_feet(g) / 3;
}

int ticks_per_exploration_turn(const Game& g) {
    return exploration_move_feet(g) / 10;
}

// ---------------------------
// Visibility / Lighting
// ---------------------------

static bool is_opaque(const Tile& t) {
    return t.type == TileType::Wall || t.type == TileType::DoorClosed;
}

static void cast_ray(Game& g, int x0, int y0, int x1, int y1) {
    int adx = std::abs(x1 - x0);
    int ady = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = adx - ady;

    int x = x0, y = y0;
    while (x != x1 || y != y1) {
        int e2 = 2 * err;
        if (e2 > -ady) { err -= ady; x += sx; }
        if (e2 < adx)  { err += adx; y += sy; }

        auto& tile = g.map[index_for(g, x, y)];
        tile.explored = true;

        if (is_opaque(tile) && (x != x1 || y != y1))
            return; // blocked — target not visible
    }
    g.map[index_for(g, x1, y1)].visible = true;
    g.map[index_for(g, x1, y1)].explored = true;
}

void compute_visibility(Game& g) {
    Entity& hero = g.hero;

    // Advance turn clock. At 120' exploration move this is 12 10-foot steps;
    // heavier encumbrance reduces steps per turn per B/X movement rates.
    ++g.tick_clock;
    bool is_turn_tick = (g.tick_clock % ticks_per_exploration_turn(g) == 0);

    if (is_turn_tick) {
        // Check torch state before decrement (for message logic)
        bool torch_was_lit = hero.torch_turns > 0 && hero.light_spell_turns == 0;

        // Decrement light sources once per turn
        if (hero.light_spell_turns > 0)
            --hero.light_spell_turns;
        else if (hero.lantern_turns > 0)
            --hero.lantern_turns;
        else if (hero.torch_turns > 0)
            --hero.torch_turns;

        // Torch status messages (in turns remaining)
        if (torch_was_lit && hero.torch_turns == 0)
            log_event(g, "Your torch sputters out!");
        else if (torch_was_lit && hero.torch_turns == 10)
            log_event(g, "Your torch is burning low.");
        else if (torch_was_lit && hero.torch_turns == 5)
            log_event(g, "Your torch is almost out!");

        // ── Hunger clock (once per B/X turn) ──
        // Every 48 B/X turns (~8 hours), attempt to eat
        hero.turns_without_food++;
        if (hero.turns_without_food > 0 && hero.turns_without_food % 48 == 0) {
            if (hero.rations > 0) {
                hero.rations--;
                hero.turns_without_food = 0;
                log_event(g, "You consume some rations.");
            } else if (hero.turns_without_food == 48) {
                log_event(g, "Your stomach rumbles — you need food.");
            } else if (hero.turns_without_food >= 144 && (hero.turns_without_food / 48) % 3 == 0) {
                // Damage on cycles 144, 288, 432, ... B/X turns
                int dmg = (hero.turns_without_food >= 192) ? roll_d(g, 4) : 1;
                hero.hp -= dmg;
                log_event(g, "You suffer " + std::to_string(dmg) + " damage from starvation!");
                if (hero.hp <= 0) {
                    hero.hp = 0;
                    hero.alive = false;
                    log_event(g, "You have starved to death.");
                }
            }
        }
    }

    // Determine light radius
    int radius = 6;
    if (hero.light_spell_turns > 0)
        radius = 10;
    else if (hero.lantern_turns > 0)
        radius = 6;
    else if (hero.torch_turns <= 0) {
        // No light source: infravision for demihumans
        switch (hero.char_class) {
            case CharacterClass::Dwarf:
            case CharacterClass::Elf:
            case CharacterClass::Halfling:
                radius = 2; // Infravision 60'
                break;
            default:
                radius = 0; // Pitch black
        }
    }

    // Reset visible flags
    for (auto& t : g.map)
        t.visible = false;

    int hx = hero.pos.x, hy = hero.pos.y;
    if (!in_bounds(g, hx, hy)) return;

    // Hero tile always explored and visible
    g.map[index_for(g, hx, hy)].explored = true;
    g.map[index_for(g, hx, hy)].visible = true;

    if (radius <= 0) return;

    // Cast rays to every tile within radius
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx == 0 && dy == 0) continue;
            if (dx * dx + dy * dy > radius * radius) continue;

            int tx = hx + dx;
            int ty = hy + dy;
            if (!in_bounds(g, tx, ty)) continue;
            if (is_opaque(g.map[index_for(g, tx, ty)])) continue;

            cast_ray(g, hx, hy, tx, ty);
        }
    }
}

// Trigger a trap on the hero
static void trigger_trap(Game& g, Entity& hero, Tile& tile) {
    if (!tile.has_trap || tile.trap_type == TrapType::None) return;
    queue_sound(g, SoundEffect::Trap);
    int dmg = 0;
    const char* msg = "";
    switch (tile.trap_type) {
        case TrapType::Pit:
            if (consume_gear(hero, "rope")) {
                log_event(g, "Your rope catches fast, sparing you the fall into the pit.");
                tile.has_trap = false;
                tile.trap_type = TrapType::None;
                return;
            }
            dmg = roll_d(g, 6);
            msg = "You fall into a pit!";
            break;
        case TrapType::Dart:
            dmg = roll_d(g, 4);
            msg = "A dart shoots out at you!";
            break;
        case TrapType::PoisonDart:
            dmg = roll_d(g, 4);
            msg = "A poisoned dart hits you!";
            // Poison save
            if (!make_save(g, hero.char_class, 0)) {
                dmg += roll_d(g, 4); // extra poison damage
            }
            break;
        default:
            break;
    }
    if (dmg > 0) {
        hero.hp -= dmg;
        {
            std::ostringstream oss;
            oss << msg << " (" << dmg << " damage)";
            log_event(g, oss.str());
        }
        if (hero.hp <= 0) {
            hero.hp    = 0;
            hero.alive = false;
            queue_sound(g, SoundEffect::PlayerDeath);
            log_event(g, "You have died.");
        }
    }
    tile.has_trap  = false;
    tile.trap_type = TrapType::None;
}

// Advance the wandering monster counter. About every two-thirds of an
// exploration turn, roll 1-in-6 for random encounters.
void tick_wander_counter(Game& g) {
    g.wander_counter++;
    int threshold = std::max(1, (ticks_per_exploration_turn(g) * 2) / 3);
    if (g.wander_counter >= threshold) {
        g.wander_counter = 0;
        if (roll_d(g, 6) == 1)
            game_internal::spawn_wandering_monsters(g);
    }
}

// Hero searches adjacent dungeon features: traps, secret doors, and noise at doors.
bool hero_search(Game& g) {
    static const int dirs[] = {-1, 0, 1, 0, 0, -1, 0, 1};
    bool found_anything = false;

    // Search for traps on adjacent tiles
    for (int i = 0; i < 4; ++i) {
        int max_range = has_gear(g.hero, "pole-10ft") ? 2 : 1;
        for (int range = 1; range <= max_range; ++range) {
            int nx = g.hero.pos.x + dirs[i * 2] * range;
            int ny = g.hero.pos.y + dirs[i * 2 + 1] * range;
            if (!in_bounds(g, nx, ny)) continue;
            auto& tile = g.map[index_for(g, nx, ny)];
            if (tile.has_trap && tile.trap_type != TrapType::None) {
                bool found = false;
                if (has_gear(g.hero, "pole-10ft")) {
                    found = true;
                } else if (g.hero.char_class == CharacterClass::Thief) {
                    found = (roll_d(g, 100) <= g.hero.skill_find_traps);
                } else {
                    found = (roll_d(g, 6) == 1); // 1-in-6 for non-thieves
                }
                if (found) {
                    tile.trap_visible = true;
                    log_event(g, range == 2
                        ? "Your 10-foot pole reveals a trap ahead!"
                        : "You find a trap!");
                    found_anything = true;
                    // Thief can try to disarm adjacent traps only.
                    if (range == 1 && g.hero.char_class == CharacterClass::Thief) {
                        if (!has_gear(g.hero, "thieves-tools")) {
                            log_event(g, "You need thieves' tools to disarm traps.");
                            continue;
                        }
                        int remove_roll = roll_d(g, 100);
                        if (remove_roll <= g.hero.skill_find_traps) {
                            tile.has_trap   = false;
                            tile.trap_type  = TrapType::None;
                            log_event(g, "You skillfully disarm the trap.");
                        } else {
                            log_event(g, "You fail to disarm the trap.");
                        }
                    }
                }
            }
        }
    }

    // Search for adjacent secret doors. B/X baseline is 1-in-6;
    // elves get 1-2 on 1d6. Thieves use their find traps skill.
    for (int i = 0; i < 4; ++i) {
        int nx = g.hero.pos.x + dirs[i * 2];
        int ny = g.hero.pos.y + dirs[i * 2 + 1];
        if (!in_bounds(g, nx, ny)) continue;
        auto& tile = g.map[index_for(g, nx, ny)];
        if (tile.type != TileType::SecretDoorClosed || tile.secret_revealed) continue;

        bool found = false;
        if (g.hero.char_class == CharacterClass::Thief) {
            found = (roll_d(g, 100) <= g.hero.skill_find_traps);
        } else if (g.hero.char_class == CharacterClass::Elf) {
            found = (roll_d(g, 6) <= 2);
        } else {
            found = (roll_d(g, 6) == 1);
        }

        if (found) {
            tile.secret_revealed = true;
            found_anything = true;
            log_event(g, "You find a secret door!");
        }
    }

    // Hear noise at adjacent doors (anyone can try, Thief uses skill)
    for (int i = 0; i < 4; ++i) {
        int nx = g.hero.pos.x + dirs[i * 2];
        int ny = g.hero.pos.y + dirs[i * 2 + 1];
        if (!in_bounds(g, nx, ny)) continue;
        auto& tile = g.map[index_for(g, nx, ny)];
        if (tile.type == TileType::DoorClosed) {
            bool heard = false;
            if (g.hero.char_class == CharacterClass::Thief) {
                heard = (roll_d(g, 100) <= g.hero.skill_hear_noise);
            } else {
                heard = (roll_d(g, 6) == 1); // 1-in-6
            }
            if (heard) {
                bool monster_near = false;
                for (const auto& m : g.monsters) {
                    if (!m.alive) continue;
                    int md_x = std::abs(m.pos.x - nx);
                    int md_y = std::abs(m.pos.y - ny);
                    if (md_x <= 1 && md_y <= 1) {
                        monster_near = true;
                        break;
                    }
                }
                if (monster_near) {
                    log_event(g, "You hear something moving behind the door!");
                } else {
                    log_event(g, "You hear nothing behind the door.");
                }
                found_anything = true;
            }
        }
    }

    if (!found_anything) {
        log_event(g, "You find nothing of interest.");
    }
    return found_anything;
}

// Update current_location_id based on hero position.
// For now, just a simple north/south split.
static void update_location_description(Game& g) {
    if (g.hero.pos.y < g.height / 2) {
        g.current_location_id = "north_room";
    } else {
        g.current_location_id = "south_room";
    }
}

// Hero takes a step in the given direction, resolving monsters, traps, stairs,
// and thief wall climbing.
bool hero_move(Game& g, int dx, int dy) {
    // Returns false when no movement action is consumed (zero step or a 10-foot
    // pole probe), so the caller keeps the wandering counter unchanged.
    if (dx == 0 && dy == 0) return false;

    int nx = g.hero.pos.x + dx;
    int ny = g.hero.pos.y + dy;

    // If a monster is there, attack that monster (with backstab if hidden)
    if (Entity* target = game_internal::monster_at(g, nx, ny)) {
        bool hero_loses_action = game_internal::resolve_first_contact_surprise(g, *target);
        if (!hero_loses_action) {
            attack(g, g.hero, *target);
            // Backstab breaks stealth
            if (g.hero.hidden) {
                g.hero.hidden = false;
                log_event(g, "You emerge from the shadows.");
            }
        }
        return true;
    } else if (in_bounds(g, nx, ny)) {
        Tile& tile = g.map[index_for(g, nx, ny)];

        // Check for trap on destination tile
        if (tile.has_trap && tile.trap_type != TrapType::None && !g.hero.hidden) {
            // 10-foot pole auto-probes: reveal trap and block movement
            if (has_gear(g.hero, "pole-10ft")) {
                tile.trap_visible = true;
                log_event(g, "Your 10-foot pole reveals a trap ahead!");
                return false;
            }
            trigger_trap(g, g.hero, tile);
        }

        TileType tt = tile.type;
        if (tt == TileType::StairsDown) {
            game_internal::descend_stairs(g);
        } else if (tt == TileType::StairsUp) {
            game_internal::ascend_stairs(g);
        } else if (tt == TileType::Wall && g.hero.char_class == CharacterClass::Thief) {
            // Climbing does not make a solid wall tile passable in the grid.
            int roll = roll_d(g, 100);
            if (roll <= g.hero.skill_climb) {
                log_event(g, "You scale the wall, but cannot pass through it.");
            } else {
                log_event(g, "You fail to climb the wall.");
            }
        } else if (tt != TileType::Wall) {
            game_internal::move_entity(g, g.hero, dx, dy);
            queue_sound(g, SoundEffect::Footstep);
            update_location_description(g);
        }

        // Move silently check: if hidden, check if monsters nearby notice
        if (g.hero.hidden) {
            for (auto& m : g.monsters) {
                if (!m.alive) continue;
                int dist = std::abs(m.pos.x - g.hero.pos.x) + std::abs(m.pos.y - g.hero.pos.y);
                if (dist <= 3) {
                    int roll = roll_d(g, 100);
                    if (roll > g.hero.skill_move_silent) {
                        g.hero.hidden = false;
                        log_event(g, "A monster notices you!");
                        break;
                    }
                }
            }
        }
    }
    return true;
}