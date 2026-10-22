#include "render_snapshot.hpp"

#include "bx_data.hpp"
#include "game.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::size_t snapshot_index_for(const GameStateSnapshot& s, int x, int y) {
    return static_cast<std::size_t>(y * s.width + x);
}

const TileView& tile_at(const GameStateSnapshot& s, int x, int y) {
    return s.tiles[snapshot_index_for(s, x, y)];
}

bool is_hidden_secret_door(const TileView& t) {
    return t.type == TileType::SecretDoorClosed && !t.secret_revealed;
}

std::string format_price_with_purse(int cost_gp, int purse_gp) {
    std::ostringstream oss;
    oss << cost_gp << " gp";
    if (purse_gp >= cost_gp)
        oss << " [afford]";
    else
        oss << " [need " << (cost_gp - purse_gp) << " gp]";
    return oss.str();
}

void put_line(const std::string& s) {
    std::cout << s << "\r\n";
}

void menu_item(const char* key, const char* label) {
    std::cout << "  [" << key << "] " << label << "\r\n";
}

std::string town_status(const GameStateSnapshot& s) {
    std::ostringstream oss;
    oss << s.hero.name << "(" << s.hero.class_name << ")"
        << "  HP:" << s.hero.hp << "/" << s.hero.max_hp
        << "  Purse:" << s.purse_text
        << "  XP:" << s.total_xp << "/" << s.next_level_xp;
    return oss.str();
}

void render_town_screen(const GameStateSnapshot& s) {
    std::cout << "\x1B[2J\x1B[H";

    switch (s.state) {
    case GameState::Town:
        std::cout << "=== Town Square ===\r\n\r\n";
        menu_item("1", "Tavern — rest, hear rumors");
        menu_item("2", "Temple — healing, donate for XP");
        menu_item("3", "The Saucy Satchel — buy supplies");
        menu_item("4", "Armorer — weapons & armor");
        menu_item("5", "Camp — rest, save, enter dungeon");
        std::cout << "\r\n" << town_status(s) << "\r\n";
        std::cout << "[x] Quit\r\n";
        break;

    case GameState::Tavern:
        std::cout << "=== The Laughing Goblin Tavern ===\r\n\r\n";
        std::cout << "\"Welcome, friend! What'll it be?\"\r\n\r\n";
        menu_item("1", "Rest (1 gp — recover 1d3 HP)");
        menu_item("2", "Listen to rumors (free)");
        std::cout << "\r\n" << town_status(s) << "\r\n";
        menu_item("t", "Return to town");
        menu_item("c", "Go to camp");
        break;

    case GameState::Temple:
        std::cout << "=== Temple of the Lawful Light ===\r\n\r\n";
        std::cout << "\"The stars speak of your coming. How may we serve?\"\r\n\r\n";
        menu_item("1", "Healing (10 gp — restore 1 HP)");
        menu_item("2", "Donate gold for XP (1 gp = 1 XP, max 100)");
        std::cout << "\r\n" << town_status(s) << "\r\n";
        menu_item("t", "Return to town");
        menu_item("c", "Go to camp");
        break;

    case GameState::Shop:
        std::cout << "=== The Saucy Satchel ===\r\n\r\n";
        std::cout << "\"Everything a bold delver needs, packed tight and priced fair.\"\r\n\r\n";
        if (s.town_submenu == 0) {
            menu_item("1", "Browse B/X adventuring gear");
            std::cout << "\r\n" << town_status(s) << "\r\n";
            menu_item("t", "Return to town");
            menu_item("c", "Go to camp");
        } else if (s.town_submenu == 4 || s.town_submenu == 5) {
            int base = (s.town_submenu == 4) ? 0 : 10;
            std::cout << "--- B/X Adventuring Gear "
                      << (s.town_submenu == 4 ? "(1/2)" : "(2/2)")
                      << " ---\r\n\r\n";
            for (int i = 0; i < 10 && base + i < static_cast<int>(bx::gear_items.size()); ++i) {
                const auto& item = bx::gear_items[static_cast<std::size_t>(base + i)];
                char key = (i == 9) ? '0' : static_cast<char>('1' + i);
                std::cout << "  [" << key << "] " << item.name
                          << " — " << format_price_with_purse(item.cost_gp, s.total_gold)
                          << "  owned:" << s.hero.gear_counts[static_cast<std::size_t>(base + i)]
                          << "\r\n";
            }
            std::cout << "\r\n" << town_status(s) << "\r\n";
            if (s.town_submenu == 4) menu_item("n", "Next page");
            else menu_item("p", "Previous page");
            menu_item("t", "Back");
        }
        break;

    case GameState::Armorer:
        std::cout << "=== The Iron Forge Armory ===\r\n\r\n";
        std::cout << "\"Armor and weapons, forged right here!\"\r\n\r\n";
        if (s.town_submenu == 0) {
            menu_item("1", "Buy Weapon");
            menu_item("2", "Buy Armor");
            menu_item("3", "Sell Equipment (sell for half price)");
            std::cout << "\r\n" << town_status(s) << "\r\n";
            menu_item("t", "Return to town");
            menu_item("c", "Go to camp");
        } else if (s.town_submenu == 1) {
            std::cout << "--- Weapons for Sale ---\r\n\r\n";
            int i = 1;
            for (const auto& w : bx::weapons) {
                if (is_weapon_allowed(s.hero.char_class, w.id)) {
                    std::cout << "  [" << i << "] " << w.name
                              << " (" << w.damage_die << ", "
                              << format_price_with_purse(w.cost_gp, s.total_gold) << ")"
                              << (w.two_handed ? " [2H]" : "")
                              << "\r\n";
                    ++i;
                }
            }
            std::cout << "\r\n" << town_status(s) << "\r\n";
            menu_item("t", "Back");
        } else if (s.town_submenu == 2) {
            std::cout << "--- Armor for Sale ---\r\n\r\n";
            for (int i = 0; i < 4; ++i) {
                const auto& a = bx::armor_items[i];
                std::string desc = std::string(a.name) + " ("
                    + format_price_with_purse(a.cost_gp, s.total_gold) + ")";
                if (a.ac > 0) desc += " AC " + std::to_string(a.ac);
                else desc += " +1 AC";
                std::cout << "  [" << (i + 1) << "] " << desc << "\r\n";
            }
            std::cout << "\r\n" << town_status(s) << "\r\n";
            menu_item("t", "Back");
        } else if (s.town_submenu == 3) {
            std::cout << "--- Sell Equipment ---\r\n\r\n";
            menu_item("1", s.hero.weapon_id.empty() ? "No weapon to sell" :
                        (std::string("Sell weapon (") + s.hero.weapon_id + ")").c_str());
            menu_item("2", s.hero.armor_id.empty() ? "No armor to sell" :
                        (std::string("Sell armor (") + s.hero.armor_id + ")").c_str());
            menu_item("3", s.hero.has_shield ? "Sell shield (5 gp)" : "No shield to sell");
            std::cout << "\r\n" << town_status(s) << "\r\n";
            menu_item("t", "Back");
        }
        break;

    case GameState::Camp:
        std::cout << "=== Camp at the Dungeon's Edge ===\r\n\r\n";
        std::cout << "A small clearing with a fire pit. The entrance to\r\n";
        std::cout << "the dungeon looms nearby.\r\n\r\n";
        menu_item("1", "Rest (free — full heal, restore spells)");
        menu_item("2", "Save game");
        menu_item("3", "Enter Dungeon");
        std::cout << "\r\n" << town_status(s) << "\r\n";
        menu_item("t", "Return to town");
        menu_item("x", "Quit");
        break;

    case GameState::CharacterSheet:
        std::cout << "=== Character Sheet ===\r\n\r\n";
        std::cout << s.hero.name << " the " << s.hero.class_name << "\r\n";
        std::cout << "Level " << s.hero_level << "  XP " << s.total_xp << "  Purse "
                  << s.purse_text << "\r\n";
        std::cout << "Encumbrance " << s.carried_weight_cn << " cn"
                  << "  Move " << s.exploration_move_feet << "' ("
                  << s.encounter_move_feet << "')\r\n";
        std::cout << "HP " << s.hero.hp << "/" << s.hero.max_hp
                  << "  AC " << s.hero.ac
                  << "  Attack +" << s.hero.attack_bonus
                  << "  Damage 1d" << s.hero.damage_die;
        if (s.hero.damage_bonus != 0)
            std::cout << (s.hero.damage_bonus > 0 ? "+" : "") << s.hero.damage_bonus;
        std::cout << "\r\n\r\n";

        std::cout << "STR " << s.hero.abilities.str
                  << "  DEX " << s.hero.abilities.dex
                  << "  CON " << s.hero.abilities.con
                  << "  INT " << s.hero.abilities.intelligence
                  << "  WIS " << s.hero.abilities.wis
                  << "  CHA " << s.hero.abilities.cha << "\r\n\r\n";

        std::cout << "Weapon: " << (s.hero.weapon_id.empty() ? "none" : s.hero.weapon_id) << "\r\n";
        std::cout << "Armor: " << (s.hero.armor_id.empty() ? "none" : s.hero.armor_id)
                  << (s.hero.has_shield ? " + shield" : "") << "\r\n\r\n";

        if (s.town_submenu == 6) {
            std::cout << "--- Equip Weapon ---\r\n\r\n";
            int shown = 1;
            for (std::size_t i = 0; i < bx::weapons.size(); ++i) {
                if (s.hero.weapon_counts[i] <= 0) continue;
                const auto& w = bx::weapons[i];
                std::cout << "  [" << shown << "] " << w.name << " (" << w.damage_die << ") owned:"
                          << s.hero.weapon_counts[i] << "\r\n";
                ++shown;
            }
            if (shown == 1) std::cout << "  No owned weapons.\r\n";
            menu_item("t", "Back");
        } else if (s.town_submenu == 7) {
            std::cout << "--- Equip Armor / Shield ---\r\n\r\n";
            menu_item("0", "Unequip armor");
            for (std::size_t i = 0; i < bx::armor_items.size(); ++i) {
                if (s.hero.armor_counts[i] <= 0) continue;
                const auto& a = bx::armor_items[i];
                std::cout << "  [" << (i + 1) << "] " << a.name << " owned:"
                          << s.hero.armor_counts[i] << "\r\n";
            }
            menu_item("t", "Back");
        } else {
            std::cout << "--- Inventory ---\r\n";
            for (std::size_t i = 0; i < bx::gear_items.size(); ++i)
                if (s.hero.gear_counts[i] > 0)
                    std::cout << "  " << bx::gear_items[i].name << ": " << s.hero.gear_counts[i] << "\r\n";
            std::cout << "\r\n";
            menu_item("1", "Equip weapon");
            menu_item("2", "Equip armor / shield");
            menu_item("t", "Return");
        }
        break;

    default:
        break;
    }

    int start = std::max(0, static_cast<int>(s.event_log.size()) - 3);
    for (int i = start; i < static_cast<int>(s.event_log.size()); ++i)
        std::cout << "  " << s.event_log[static_cast<std::size_t>(i)] << "\r\n";

    std::cout << "\r\n> " << std::flush;
}

void render_dungeon(const GameStateSnapshot& s) {
    std::cout << "\x1B[2J\x1B[H";
    for (int y = 0; y < s.height; ++y) {
        for (int x = 0; x < s.width; ++x) {
            const TileView& t = tile_at(s, x, y);

            if (!t.explored) {
                std::cout << ' ';
                continue;
            }

            if (!t.visible)
                std::cout << "\x1B[90m";

            if (t.type == TileType::Wall || is_hidden_secret_door(t)) {
                if (t.visible_wall)
                    std::cout << t.wall_glyph;
                else
                    std::cout << ' ';
            } else {
                char ch = '.';
                if (t.type == TileType::DoorClosed) ch = '+';
                else if (t.type == TileType::SecretDoorClosed) ch = '+';
                else if (t.type == TileType::DoorOpen) ch = '/';
                else if (t.type == TileType::StairsDown) ch = '>';
                else if (t.type == TileType::StairsUp) ch = '<';

                if (t.visible) {
                    if (s.hero.alive && s.hero.pos.x == x && s.hero.pos.y == y) {
                        ch = '@';
                    } else {
                        for (const auto& m : s.monsters) {
                            if (!m.alive) continue;
                            if (m.pos.x == x && m.pos.y == y) {
                                ch = m.glyph;
                                break;
                            }
                        }
                    }
                }

                if (ch == '.' && !t.items.empty()) {
                    ItemType top = t.items.back();
                    if (top == ItemType::ScrollArcane || top == ItemType::ScrollClerical)
                        ch = '?';
                    else if (top == ItemType::Gold)
                        ch = '$';
                    else if (top == ItemType::Gem)
                        ch = '*';
                    else
                        ch = '!';
                }

                if (ch == '.' && t.trap_visible && t.has_trap)
                    ch = '^';

                std::cout << ch;
            }

            if (!t.visible)
                std::cout << "\x1B[39m";
        }
        std::cout << "\r\n";
    }

    std::ostringstream oss;
    oss << s.hero.name << "(" << s.hero.class_name << ")"
        << " HP:" << s.hero.hp << "/" << s.hero.max_hp
        << " AC:" << s.hero.ac;
    if (s.hero.effective_ac != s.hero.ac)
        oss << "(" << s.hero.effective_ac << ":" << s.hero.buff_turns << ")";
    oss << " Lv." << s.hero_level
        << " D:" << s.dungeon_level
        << " Mv:" << s.exploration_move_feet << "'"
        << " Purse:" << s.purse_text
        << " XP:" << s.total_xp << "/" << s.next_level_xp
        << " M:" << s.alive_monster_count
        << " K:" << s.hero.knives
        << " D:" << s.hero.darts;
    if (s.hero.light_spell_turns > 0)
        oss << " L*:" << s.hero.light_spell_turns;
    else if (s.hero.lantern_turns > 0)
        oss << " Ln:" << s.hero.lantern_turns;
    else
        oss << " T:" << s.hero.torch_turns;
    if (s.hero.turns_without_food >= 18)
        oss << " STARVING!";
    else if (s.hero.turns_without_food >= 12)
        oss << " Hungry";
    if (s.hero.confused)
        oss << " CONFUSED!";
    if (s.hero.unconscious_turns > 0)
        oss << " UNCONSCIOUS!";
    if (s.hero.blister_turns > 0)
        oss << " BLISTERS (-2 to hit)";
    oss << " R:" << s.hero.rations;
    put_line(oss.str());

    int start = std::max(0, static_cast<int>(s.event_log.size()) - 2);
    for (int i = start; i < static_cast<int>(s.event_log.size()); ++i)
        std::cout << "  " << s.event_log[static_cast<std::size_t>(i)] << "\r\n";

    std::cout << "\x1B[J> " << std::flush;
}

} // namespace

void render(const GameStateSnapshot& snapshot) {
    if (snapshot.state != GameState::Dungeon) {
        render_town_screen(snapshot);
        return;
    }
    render_dungeon(snapshot);
}
