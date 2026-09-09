#include "game.hpp"
#include "game_internal.hpp"
#include "map.hpp"
#include "bx_data.hpp"
#include "item_effects.hpp"

#include <algorithm>
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

Entity* monster_at(Game& g, int x, int y) {
    return game_internal::monster_at(g, x, y);
}

void award_treasure(Game& g, Entity& monster) {
    game_internal::award_treasure(g, monster);
}

void check_level_up(Game& g) {
    game_internal::check_level_up(g);
}

bool is_undead(const Entity& e) {
    if (e.type == EntityType::Monster) return e.undead;
    return e.type == EntityType::Skeleton || e.type == EntityType::Zombie;
}

int spell_level_by_name(const std::string& name) {
    for (const auto& s : bx::spells) {
        if (name == s.name) return s.level;
    }
    return 0;
}

bool is_cleric_spell_name(const std::string& name) {
    for (const auto& s : bx::spells) {
        if (name == s.name && std::string_view(s.class_name) == "Cleric")
            return true;
    }
    return false;
}

} // namespace

bool cast_spell(Game& g, int spell_index, int dx, int dy) {
    Entity& hero = g.hero;
    if (spell_index < 0 || spell_index >= static_cast<int>(hero.spells.size())) {
        log_event(g, "That spell is not known.");
        return false;
    }
    const std::string& spell = hero.spells[static_cast<std::size_t>(spell_index)];

    if (hero.paralyzed_turns > 0 && spell != "Cure Light Wounds") {
        log_event(g, "You are too stiff to cast that spell!");
        return false;
    }

    if (hero.unconscious_turns > 0) {
        log_event(g, "You are unconscious and cannot cast spells!");
        return false;
    }

    if (hero_is_confused(g)) {
        log_event(g, "The bats' fluttering confuses your concentration!");
        return false;
    }

    if (hero.char_class == CharacterClass::Cleric && is_cleric_spell_name(spell)
        && !has_gear(hero, "holy-symbol")) {
        log_event(g, "You need a holy symbol to cast cleric spells.");
        return false;
    }

    int spell_level = spell_level_by_name(spell);
    if (spell_level < 1 || spell_level > Entity::MaxSpellLevel) {
        log_event(g, "Unknown spell level.");
        return false;
    }
    if (hero.spell_slots[spell_level] <= 0) {
        log_event(g, "No spell slots remaining!");
        return false;
    }

    queue_sound(g, SoundEffect::Spell);

    if (spell == "Magic Missile") {
        if (dx == 0 && dy == 0) {
            log_event(g, "Magic Missile needs a direction!");
            return false;
        }
        int dist = 1;
        int nx = hero.pos.x + dx;
        int ny = hero.pos.y + dy;
        while (in_bounds(g, nx, ny) && dist <= 4) {
            if (is_blocked(g, nx, ny))
                break;
            if (Entity* target = monster_at(g, nx, ny)) {
                --hero.spell_slots[spell_level];
                int dmg = roll_d(g, 4) + 1;
                target->hp -= dmg;
                {
                    std::ostringstream oss;
                    oss << "Magic Missile hits " << target->name
                        << " for " << dmg << " damage!";
                    log_event(g, oss.str());
                }
                if (target->hp <= 0) {
                    target->hp = 0;
                    target->alive = false;
                    log_event(g, target->name + " is slain!");
                    award_treasure(g, *target);
                    check_level_up(g);
                }
                return true;
            }
            nx += dx;
            ny += dy;
            ++dist;
        }
        log_event(g, "Magic Missile flies into the darkness.");
        return false;
    }

    if (spell == "Sleep") {
        --hero.spell_slots[spell_level];
        int hd_affected = roll_d(g, 4) + roll_d(g, 4);
        int affected = 0;
        for (auto& m : g.monsters) {
            if (!m.alive) continue;
            if (is_undead(m)) continue;
            if (affected >= hd_affected * 2) break;
            m.hp = 0;
            m.alive = false;
            ++affected;
            log_event(g, m.name + " falls asleep!");
        }
        if (affected == 0) {
            log_event(g, "Sleep has no effect.");
        }
        return true;
    }

    if (spell == "Light") {
        --hero.spell_slots[spell_level];
        hero.light_spell_turns = 20;
        log_event(g, "A magical light illuminates the area.");
        return true;
    }

    if (spell == "Cure Light Wounds") {
        --hero.spell_slots[spell_level];
        int heal = roll_d(g, 6) + 1;
        int actual = std::min(heal, hero.max_hp - hero.hp);
        hero.hp += actual;
        {
            std::ostringstream oss;
            oss << "Cure Light Wounds heals " << actual
                << " HP (" << hero.hp << "/" << hero.max_hp << ").";
            log_event(g, oss.str());
        }
        if (hero.paralyzed_turns > 0) {
            hero.paralyzed_turns = 0;
            log_event(g, "The spell also cures your paralysis!");
        }
        return true;
    }

    if (spell == "Read Magic") {
        --hero.spell_slots[spell_level];
        log_event(g, "You study the arcane sigils — they become clear.");
        return true;
    }

    if (spell == "Charm Person") {
        if (dx == 0 && dy == 0) {
            log_event(g, "Charm Person needs a direction!");
            return false;
        }
        int nx = hero.pos.x + dx;
        int ny = hero.pos.y + dy;
        Entity* target = monster_at(g, nx, ny);
        if (!target) {
            log_event(g, "No one there to charm.");
            return false;
        }
        if (is_undead(*target)) {
            log_event(g, "Undead are immune to Charm Person.");
            return false;
        }
        --hero.spell_slots[spell_level];
        target->charmed = true;
        log_event(g, target->name + " is charmed!");
        return true;
    }

    if (spell == "Detect Magic") {
        --hero.spell_slots[spell_level];
        log_event(g, "You detect magical auras in the area.");
        return true;
    }

    if (spell == "Floating Disc") {
        --hero.spell_slots[spell_level];
        log_event(g, "A floating disc materialises and follows you.");
        return true;
    }

    if (spell == "Hold Portal") {
        static const int dirs[] = {-1, 0, 1, 0, 0, -1, 0, 1};
        bool held = false;
        for (int i = 0; i < 4; ++i) {
            int nx = hero.pos.x + dirs[i * 2];
            int ny = hero.pos.y + dirs[i * 2 + 1];
            if (!in_bounds(g, nx, ny)) continue;
            auto& tile = g.map[index_for(g, nx, ny)];
            if (tile.type == TileType::DoorClosed) {
                tile.held = true;
                held = true;
                log_event(g, "The door is magically held!");
                break;
            }
        }
        if (!held) {
            log_event(g, "No adjacent door to hold.");
            return false;
        }
        --hero.spell_slots[spell_level];
        return true;
    }

    if (spell == "Protection from Evil") {
        --hero.spell_slots[spell_level];
        hero.protection_turns = 6;
        log_event(g, "A protective aura surrounds you.");
        return true;
    }

    if (spell == "Shield") {
        --hero.spell_slots[spell_level];
        hero.shield_turns = 6;
        log_event(g, "A shimmering shield appears before you.");
        return true;
    }

    if (spell == "Ventriloquism") {
        --hero.spell_slots[spell_level];
        log_event(g, "You throw your voice to confuse the monsters.");
        return true;
    }

    if (spell == "Detect Evil") {
        --hero.spell_slots[spell_level];
        int count = 0;
        for (auto& m : g.monsters)
            if (m.alive) ++count;
        {
            std::ostringstream oss;
            oss << "You detect " << count << " evil presence"
                << (count == 1 ? "" : "s") << " nearby.";
            log_event(g, oss.str());
        }
        return true;
    }

    if (spell == "Purify Food and Water") {
        --hero.spell_slots[spell_level];
        int heal = roll_d(g, 4);
        int actual = std::min(heal, hero.max_hp - hero.hp);
        hero.hp += actual;
        {
            std::ostringstream oss;
            oss << "Purify Food and Water restores " << actual
                << " HP (" << hero.hp << "/" << hero.max_hp << ").";
            log_event(g, oss.str());
        }
        return true;
    }

    if (spell == "Remove Fear") {
        --hero.spell_slots[spell_level];
        hero.courage_turns = 6;
        log_event(g, "Divine courage fills you — +1 to attack rolls.");
        return true;
    }

    if (spell == "Resist Cold") {
        --hero.spell_slots[spell_level];
        hero.resist_cold_turns = 6;
        log_event(g, "A cold-resistant aura surrounds you.");
        return true;
    }

    if (spell == "Continual Light") {
        --hero.spell_slots[spell_level];
        hero.light_spell_turns = 999;
        log_event(g, "A permanent magical glow fills the area.");
        return true;
    }

    if (spell == "Detect Invisible") {
        --hero.spell_slots[spell_level];
        log_event(g, "You can now see invisible creatures.");
        return true;
    }

    if (spell == "ESP") {
        --hero.spell_slots[spell_level];
        int count = 0;
        for (auto& m : g.monsters)
            if (m.alive) ++count;
        {
            std::ostringstream oss;
            oss << "You sense " << count << " thinking mind"
                << (count == 1 ? "" : "s") << " nearby.";
            log_event(g, oss.str());
        }
        return true;
    }

    if (spell == "Invisibility") {
        --hero.spell_slots[spell_level];
        hero.hidden = true;
        log_event(g, "You fade from sight!");
        return true;
    }

    if (spell == "Knock") {
        --hero.spell_slots[spell_level];
        static const int dirs[] = {-1, 0, 1, 0, 0, -1, 0, 1};
        bool opened = false;
        for (int i = 0; i < 4; ++i) {
            int nx = hero.pos.x + dirs[i * 2];
            int ny = hero.pos.y + dirs[i * 2 + 1];
            if (!in_bounds(g, nx, ny)) continue;
            auto& tile = g.map[index_for(g, nx, ny)];
            if (tile.type == TileType::DoorClosed) {
                tile.type = TileType::DoorOpen;
                tile.held = false;
                opened = true;
                log_event(g, "The door swings open!");
                break;
            }
        }
        if (!opened)
            log_event(g, "No locked door adjacent.");
        return true;
    }

    if (spell == "Levitate") {
        --hero.spell_slots[spell_level];
        log_event(g, "You rise gently into the air.");
        return true;
    }

    if (spell == "Locate Object") {
        --hero.spell_slots[spell_level];
        log_event(g, "You concentrate on a known object...");
        return true;
    }

    if (spell == "Mirror Image") {
        --hero.spell_slots[spell_level];
        log_event(g, "Mirror images of you shimmer into being.");
        return true;
    }

    if (spell == "Phantasmal Force") {
        --hero.spell_slots[spell_level];
        log_event(g, "An illusion shimmers into existence.");
        return true;
    }

    if (spell == "Web") {
        --hero.spell_slots[spell_level];
        int webbed = 0;
        for (auto& m : g.monsters) {
            if (!m.alive) continue;
            m.fleeing = false;
            ++webbed;
        }
        {
            std::ostringstream oss;
            oss << "Thick webs entangle " << webbed << " monster"
                << (webbed == 1 ? "" : "s") << "!";
            log_event(g, oss.str());
        }
        return true;
    }

    if (spell == "Wizard Lock") {
        --hero.spell_slots[spell_level];
        static const int dirs[] = {-1, 0, 1, 0, 0, -1, 0, 1};
        bool locked = false;
        for (int i = 0; i < 4; ++i) {
            int nx = hero.pos.x + dirs[i * 2];
            int ny = hero.pos.y + dirs[i * 2 + 1];
            if (!in_bounds(g, nx, ny)) continue;
            auto& tile = g.map[index_for(g, nx, ny)];
            if (tile.type == TileType::DoorClosed) {
                tile.held = true;
                locked = true;
                log_event(g, "The door is wizard-locked!");
                break;
            }
        }
        if (!locked)
            log_event(g, "No adjacent door to lock.");
        return true;
    }

    if (spell == "Bless") {
        --hero.spell_slots[spell_level];
        int heal = roll_d(g, 4);
        int actual = std::min(heal, hero.max_hp - hero.hp);
        hero.hp += actual;
        hero.courage_turns = std::max(hero.courage_turns, 6);
        {
            std::ostringstream oss;
            oss << "Bless restores " << actual
                << " HP and emboldens you (+1 attack).";
            log_event(g, oss.str());
        }
        return true;
    }

    if (spell == "Hold Person") {
        --hero.spell_slots[spell_level];
        int held = 0;
        for (auto& m : g.monsters) {
            if (!m.alive) continue;
            if (m.morale <= 0) continue;
            m.alive = false;
            ++held;
        }
        {
            std::ostringstream oss;
            oss << "Hold Person paralyzes " << held << " humanoid"
                << (held == 1 ? "" : "s") << "!";
            log_event(g, oss.str());
        }
        return true;
    }

    if (spell == "Silence 15' radius") {
        --hero.spell_slots[spell_level];
        log_event(g, "An eerie silence envelops the area.");
        return true;
    }

    if (spell == "Dispel Magic") {
        --hero.spell_slots[spell_level];
        hero.shield_turns = 0;
        hero.protection_turns = 0;
        hero.courage_turns = 0;
        hero.resist_cold_turns = 0;
        hero.light_spell_turns = 0;
        for (auto& tile : g.map)
            tile.held = false;
        log_event(g, "Magic dissipates around you — all enchantments end.");
        return true;
    }

    if (spell == "Fire Ball") {
        --hero.spell_slots[spell_level];
        int killed = 0;
        for (auto& m : g.monsters) {
            if (!m.alive) continue;
            int dmg = roll_d(g, 6) + roll_d(g, 6);
            m.hp -= dmg;
            if (m.hp <= 0) {
                m.hp = 0;
                m.alive = false;
                ++killed;
                log_event(g, m.name + " is consumed by the fire ball!");
            } else {
                log_event(g, m.name + " is scorched by the fire ball!");
            }
        }
        {
            std::ostringstream oss;
            oss << "Fire ball kills " << killed << " monster"
                << (killed == 1 ? "" : "s") << "!";
            log_event(g, oss.str());
        }
        return true;
    }

    if (spell == "Fly") {
        --hero.spell_slots[spell_level];
        log_event(g, "You take to the air!");
        return true;
    }

    if (spell == "Read Languages") {
        --hero.spell_slots[spell_level];
        log_event(g, "You can read all languages for a short time.");
        return true;
    }

    {
        std::ostringstream oss;
        oss << spell << " is not a known spell.";
        log_event(g, oss.str());
    }
    return false;
}
