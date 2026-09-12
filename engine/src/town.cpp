#include "game.hpp"
#include "game_internal.hpp"
#include "inventory.hpp"
#include "bx_data.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace {

void log_event(Game& g, const std::string& msg) {
    game_internal::emit_log_event(g, msg);
}

void open_character_sheet(Game& g) {
    if (g.state != GameState::CharacterSheet)
        g.previous_state = g.state;
    g.state = GameState::CharacterSheet;
    g.town_submenu = 0;
}

void return_from_character_sheet(Game& g) {
    g.state = g.previous_state;
    g.town_submenu = 0;
}

void buy_gear_item(Game& g, int gear_index) {
    if (gear_index < 0 || gear_index >= static_cast<int>(bx::gear_items.size())) return;

    const auto& item = bx::gear_items[static_cast<std::size_t>(gear_index)];
    if (g.total_gold < item.cost_gp) {
        log_event(g, std::string("Not enough gold. ") + item.name + " costs "
                    + std::to_string(item.cost_gp) + " gp.");
        return;
    }

    g.total_gold -= item.cost_gp;
    give_item(g.hero, item.id);

    std::string_view id(item.id);
    if (id == "standard-rations") {
        g.hero.rations += 21;
    } else if (id == "iron-rations") {
        g.hero.rations += 42;
    } else if (id == "oil-flask") {
        g.hero.oil_flasks++;
    } else if (id == "holy-water") {
        g.hero.holy_water++;
    } else if (id == "torches") {
        g.hero.torch_turns += 120; // six torches, each worth the current 20-turn torch unit
    }

    log_event(g, std::string("You buy ") + item.name + " for "
                + std::to_string(item.cost_gp) + " gp.");
}

} // namespace

// ── Town command handler ──────────────────────────────────

bool apply_town_command(Game& g, const PlayerCommand& cmd) {
    if (cmd.type == PlayerCommandType::Quit)
        return false;

    if (cmd.type == PlayerCommandType::Inventory) {
        open_character_sheet(g);
        return true;
    }

    auto set_state = [&](GameState s) {
        g.state = s;
        g.town_submenu = 0;
        g.event_log.clear();
    };

    switch (g.state) {
    // ─── Character Sheet ───────────────────────────────────
    case GameState::CharacterSheet:
        if (cmd.type == PlayerCommandType::Help) {
            if (g.town_submenu != 0) g.town_submenu = 0;
            else return_from_character_sheet(g);
            return true;
        }
        if (cmd.type == PlayerCommandType::Get) {
            if (g.town_submenu == 0) {
                if (cmd.spell_index == 0) g.town_submenu = 6;
                else if (cmd.spell_index == 1) g.town_submenu = 7;
            } else if (g.town_submenu == 6) {
                game_internal::equip_owned_weapon(g, cmd.spell_index);
            } else if (g.town_submenu == 7) {
                game_internal::equip_owned_armor(g, cmd.spell_index);
            }
            return true;
        }
        return true;

    // ─── Town ──────────────────────────────────────────────
    case GameState::Town:
        switch (cmd.type) {
            case PlayerCommandType::Quit:     return false;
            case PlayerCommandType::Move:
                if      (cmd.dx == 0 && cmd.dy == -1) set_state(GameState::Tavern);
                else if (cmd.dx == -1 && cmd.dy == 0) set_state(GameState::Temple);
                else if (cmd.dx == 1  && cmd.dy == 0) set_state(GameState::Shop);
                else if (cmd.dx == -1 && cmd.dy == 1) set_state(GameState::Armorer);
                else if (cmd.dx == 0 && cmd.dy == 1)  set_state(GameState::Camp);
                break;
            default: break;
        }
        return true;

    // ─── Tavern ────────────────────────────────────────────
    case GameState::Tavern:
        switch (cmd.type) {
            case PlayerCommandType::Rest: {
                if (g.total_gold < 1) {
                    log_event(g, "You don't have a single gold piece to pay for a room.");
                } else {
                    --g.total_gold;
                    int heal = roll_d(g, 3);
                    g.hero.hp += heal;
                    if (g.hero.hp > g.hero.max_hp) g.hero.hp = g.hero.max_hp;
                    log_event(g, "You rent a room for 1 gp and rest, recovering "
                                + std::to_string(heal) + " HP.");
                }
                return true;
            }
            case PlayerCommandType::Listen: {
                // Serve the next module rumor the campaign has not heard yet, in
                // declaration order; once every rumor is out, the barkeep has
                // nothing new to share.
                const auto next = std::find_if(
                    g.module_rumors.begin(), g.module_rumors.end(),
                    [&g](const GameRumor& rumor) {
                        return std::find(g.heard_rumors.begin(), g.heard_rumors.end(),
                                         rumor.rumor_id) == g.heard_rumors.end();
                    });
                if (next == g.module_rumors.end()) {
                    log_event(g, "The barkeep has no rumors to share with you.");
                } else {
                    g.heard_rumors.push_back(next->rumor_id);
                    log_event(g, next->text);
                }
                return true;
            }
            default: break;
        }
        // Navigation from tavern
        if (cmd.type == PlayerCommandType::Move) {
            if (cmd.dx == 2) set_state(GameState::Town);
            else if (cmd.dy == 1) set_state(GameState::Camp);
            return true;
        }
        return true;

    // ─── Temple ────────────────────────────────────────────
    case GameState::Temple:
        switch (cmd.type) {
            case PlayerCommandType::Cast: {
                int cost = 10;
                if (g.total_gold < cost) {
                    log_event(g, "You cannot afford the 10 gp donation for healing.");
                } else if (g.hero.hp >= g.hero.max_hp) {
                    log_event(g, "You are already at full health.");
                } else {
                    g.total_gold -= cost;
                    g.hero.hp += 1;
                    if (g.hero.hp > g.hero.max_hp) g.hero.hp = g.hero.max_hp;
                    log_event(g, "You donate 10 gp — the clerics restore 1 HP.");
                }
                return true;
            }
            case PlayerCommandType::Turn: {
                int amount = std::min(g.total_gold, 100);
                if (amount <= 0) {
                    log_event(g, "You have no gold to donate.");
                } else {
                    g.total_gold -= amount;
                    g.total_xp += amount;
                    log_event(g, "You donate " + std::to_string(amount)
                                + " gp to the temple and gain "
                                + std::to_string(amount) + " XP.");
                }
                return true;
            }
            default: break;
        }
        // Navigation from temple
        if (cmd.type == PlayerCommandType::Move) {
            if (cmd.dx == 2) set_state(GameState::Town);
            else if (cmd.dy == 1) set_state(GameState::Camp);
            return true;
        }
        return true;

    // ─── Shop ──────────────────────────────────────────────
    case GameState::Shop:
        // Back from submenu
        if (cmd.type == PlayerCommandType::Help && g.town_submenu != 0) {
            g.town_submenu = 0;
            return true;
        }
        switch (cmd.type) {
            case PlayerCommandType::Get:
                if (g.town_submenu == 4 || g.town_submenu == 5) {
                    buy_gear_item(g, cmd.spell_index);
                    g.town_submenu = 0;
                }
                return true;
            case PlayerCommandType::Use:
                // Browse B/X gear
                g.town_submenu = 4;
                return true;
            case PlayerCommandType::Open:
                if (g.town_submenu == 4) g.town_submenu = 5;
                return true;
            case PlayerCommandType::Close:
                if (g.town_submenu == 5) g.town_submenu = 4;
                return true;
            default: break;
        }
        // Navigation from shop
        if (cmd.type == PlayerCommandType::Move) {
            if (cmd.dx == 2) set_state(GameState::Town);
            else if (cmd.dy == 1) set_state(GameState::Camp);
            return true;
        }
        return true;

    // ─── Armorer ───────────────────────────────────────────
    case GameState::Armorer:
        // Back from submenu
        if (cmd.type == PlayerCommandType::Help && g.town_submenu != 0) {
            g.town_submenu = 0;
            return true;
        }
        switch (cmd.type) {
            case PlayerCommandType::Get: {
                if (g.town_submenu == 0) {
                    // Option 1: buy weapon -> enters submenu
                    g.town_submenu = 1;
                } else if (g.town_submenu == 1) {
                    // Weapon selection
                    int idx = cmd.spell_index;
                    int count = 0;
                    for (std::size_t wi = 0; wi < bx::weapons.size(); ++wi) {
                        const auto& w = bx::weapons[wi];
                        if (is_weapon_allowed(g.hero.char_class, w.id)) {
                            if (count == idx) {
                                if (g.total_gold >= w.cost_gp) {
                                    g.total_gold -= w.cost_gp;
                                    g.hero.weapon_counts[wi]++;
                                    g.hero.weapon_id = w.id;
                                    g.hero.damage_die = game_internal::parse_die_size(w.damage_die);
                                    // Two-handed: can't use shield
                                    if (w.two_handed) g.hero.has_shield = false;
                                    log_event(g, std::string("You buy a ") + w.name + " for " + std::to_string(w.cost_gp) + " gp.");
                                } else {
                                    log_event(g, std::string("You can't afford a ") + w.name + " (" + std::to_string(w.cost_gp) + " gp).");
                                }
                                break;
                            }
                            ++count;
                        }
                    }
                    g.town_submenu = 1;
                } else if (g.town_submenu == 2) {
                    // Armor selection
                    int idx = cmd.spell_index;
                    if (idx >= 0 && idx < 4) {
                        const auto& a = bx::armor_items[idx];
                        if (g.total_gold >= a.cost_gp) {
                            // Check class restriction
                            bool allowed = false;
                            if (std::string_view(a.armor_type) == "light") {
                                allowed = true; // light armor allowed for all
                            } else if (std::string_view(a.id) == "shield") {
                                if (game_internal::can_use_shield(g.hero.char_class)) allowed = true;
                            } else {
                                if (game_internal::can_wear_heavy_armor(g.hero.char_class)) allowed = true;
                            }
                            if (!allowed) {
                                log_event(g, std::string("Your class cannot wear ") + a.name + ".");
                            } else if (a.ac > 0 && g.hero.ac <= a.ac) {
                                log_event(g, std::string("You already have armor as good as or better than ") + a.name + ".");
                            } else if (std::string_view(a.id) == "shield") {
                                if (g.hero.weapon_id.empty()) {
                                    log_event(g, "You need a weapon to use a shield.");
                                } else {
                                    // Check if current weapon is two-handed
                                    bool two_handed = false;
                                    for (const auto& w : bx::weapons) {
                                        if (w.id == g.hero.weapon_id && w.two_handed) {
                                            two_handed = true; break;
                                        }
                                    }
                                    if (two_handed) {
                                        log_event(g, "You can't use a shield with a two-handed weapon.");
                                    } else {
                                        g.total_gold -= a.cost_gp;
                                        g.hero.armor_counts[static_cast<std::size_t>(idx)]++;
                                        g.hero.has_shield = true;
                                        log_event(g, std::string("You buy a ") + a.name + " for " + std::to_string(a.cost_gp) + " gp.");
                                    }
                                }
                            } else {
                                g.total_gold -= a.cost_gp;
                                g.hero.armor_counts[static_cast<std::size_t>(idx)]++;
                                g.hero.armor_id = a.id;
                                g.hero.ac = a.ac;
                                log_event(g, std::string("You buy ") + a.name + " for " + std::to_string(a.cost_gp) + " gp.");
                            }
                        } else {
                            log_event(g, std::string("You can't afford ") + a.name + " (" + std::to_string(a.cost_gp) + " gp).");
                        }
                    }
                    g.town_submenu = 2;
                } else if (g.town_submenu == 3) {
                    // Sell selection
                    int idx = cmd.spell_index;
                    if (idx == 0 && !g.hero.weapon_id.empty()) {
                        for (const auto& w : bx::weapons) {
                            if (w.id == g.hero.weapon_id) {
                                int price = w.cost_gp / 2;
                                g.total_gold += price;
                                log_event(g, std::string("You sell your ") + w.name + " for " + std::to_string(price) + " gp.");
                                g.hero.weapon_id.clear();
                                g.hero.damage_die = 2; // unarmed: 1d2
                                break;
                            }
                        }
                    } else if (idx == 1 && !g.hero.armor_id.empty()) {
                        for (const auto& a : bx::armor_items) {
                            if (a.id == g.hero.armor_id) {
                                int price = a.cost_gp / 2;
                                g.total_gold += price;
                                log_event(g, std::string("You sell your ") + a.name + " for " + std::to_string(price) + " gp.");
                                g.hero.armor_id.clear();
                                g.hero.ac = 9; // unarmored
                                break;
                            }
                        }
                    } else if (idx == 2 && g.hero.has_shield) {
                        g.total_gold += 5;
                        g.hero.has_shield = false;
                        log_event(g, "You sell your shield for 5 gp.");
                    } else {
                        log_event(g, "Nothing to sell.");
                    }
                    g.town_submenu = 0;
                }
                return true;
            }
            default: break;
        }
        // Navigation from armorer
        if (cmd.type == PlayerCommandType::Move) {
            if (cmd.dx == 2) set_state(GameState::Town);
            else if (cmd.dy == 1) set_state(GameState::Camp);
            return true;
        }
        return true;

    // ─── Camp ──────────────────────────────────────────────
    case GameState::Camp:
        switch (cmd.type) {
            case PlayerCommandType::Rest:
                g.hero.hp = g.hero.max_hp;
                g.hero.torch_turns = 60;
                for (int i = 1; i <= Entity::MaxSpellLevel; ++i)
                    g.hero.spell_slots[i] = g.hero.max_spell_slots[i];
                log_event(g, "You rest at camp and recover fully.");
                return true;
            case PlayerCommandType::Help: {
                auto save_path = save_path_for_hero(g.hero.name.empty() ? "hero" : g.hero.name);
                if (save_game(g, save_path)) {
                    log_event(g, "Game saved to " + save_path.string() + ".");
                }
                else
                    log_event(g, "Failed to save game at " + save_path.string() + ".");
                return true;
            }
            case PlayerCommandType::Move:
                if (cmd.dy == -1) {
                    enter_dungeon(g);
                    return true;
                } else if (cmd.dy == 1) {
                    set_state(GameState::Town);
                    return true;
                }
                break;
            default: break;
        }
        return true;

    default:
        return true;
    }
}