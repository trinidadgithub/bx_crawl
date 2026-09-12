#include "bx_data.hpp"
#include "game.hpp"
#include "entity.hpp"
#include "item_catalog.hpp"
#include <iostream>
#include <random>
#include <filesystem>

static int roll_d6(std::mt19937& rng) {
    std::uniform_int_distribution<int> d(1, 6);
    return d(rng);
}

static int roll_3d6(std::mt19937& rng) {
    return roll_d6(rng) + roll_d6(rng) + roll_d6(rng);
}

static int roll_hp(int hd_sides, int con_mod, std::mt19937& rng) {
    std::uniform_int_distribution<int> d(1, hd_sides);
    return std::max(1, d(rng) + con_mod);
}

// Standard B/X starting gear
static void give_starting_gear(Entity& e) {
    e.gear_counts[*bx::gear_index("backpack")]  = 1;
    e.gear_counts[*bx::gear_index("torches")]   = 1; // torches (6)
    e.gear_counts[*bx::gear_index("tinder-box")] = 1;
    e.gear_counts[*bx::gear_index("rope")]      = 1;
    e.rations = 21; // one standard ration = 21 units (21 × 48 = 1008 turns ≈ 1 week)
    e.gear_counts[*bx::gear_index("waterskin")] = 1;
    e.oil_flasks = 1;
    e.gear_counts[*bx::gear_index("oil-flask")] = 1;
    // small sack, iron spikes
    e.gear_counts[*bx::gear_index("sack-small")] = 1;
    e.gear_counts[*bx::gear_index("iron-spikes")] = 1;
}

// Generate and save a pregenerated character
static bool generate_and_save(
    const std::string& name,
    CharacterClass cc,
    const Abilities& abilities,
    const char* weapon_id,
    const char* armor_id,
    bool has_shield,
    int hp,
    int atk_bonus,
    int dmg_die,
    int dmg_bonus,
    int ac,
    std::mt19937& rng,
    const std::filesystem::path& out_dir
) {
    Game g;
    init_game_runtime(g);

    Entity h;
    h.name           = name;
    h.type           = EntityType::Hero;
    h.alive          = true;
    h.max_hp         = hp;
    h.hp             = hp;
    h.attack_bonus   = atk_bonus;
    h.damage_die     = dmg_die;
    h.damage_bonus   = dmg_bonus;
    h.ac             = ac;
    h.abilities      = abilities;
    h.char_class     = cc;
    h.torch_turns    = 60;
    h.morale         = 0;
    h.fleeing        = false;
    h.attacks_per_turn = 1;

    // Weapon
    h.weapon_id = weapon_id;
    if (auto wi = bx::weapon_index(weapon_id)) h.weapon_counts[*wi] = 1;

    // Armor
    if (armor_id && std::string_view(armor_id) != "none") {
        h.armor_id = armor_id;
        if (auto ai = bx::armor_index(armor_id)) h.armor_counts[*ai] = 1;
    } else {
        h.armor_id = "none";
    }
    h.has_shield = has_shield;
    if (has_shield) h.armor_counts[*bx::armor_index("shield")] = 1;

    // Starting gear
    give_starting_gear(h);

    // Thief skills
    h.skill_hide = 10;
    h.skill_pick_locks = 15;
    h.skill_find_traps = 10;
    h.skill_climb = 85;
    h.skill_move_silent = 20;
    h.skill_hear_noise = 33;

    // Starting gold (NOT placed into purse — it's in total_gold)
    g.total_gold = roll_3d6(rng) * 10;

    g.hero = h;
    g.state = GameState::Town;

    auto path = out_dir / (name + ".dat");
    bool ok = save_game(g, path);
    if (ok)
        std::cout << "  Saved " << path << "\n";
    else
        std::cout << "  FAILED " << path << "\n";
    return ok;
}

int main() {
    std::mt19937 rng{42};
    std::filesystem::path data_dir = std::filesystem::path{"data"};
    auto out_dir = pregenerated_saves_directory(data_dir);
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    if (ec) {
        std::cerr << "Failed to create " << out_dir << ": " << ec.message() << "\n";
        return 1;
    }

    int success = 0;
    int total = 0;

    // Helper lambda
    auto gen = [&](const std::string& name, CharacterClass cc,
                   const Abilities& abilities,
                   const char* weapon_id, const char* armor_id,
                   bool shield,
                   int hp, int atk_bonus, int dmg_die, int dmg_bonus, int ac) {
        ++total;
        bool ok = generate_and_save(name, cc, abilities, weapon_id, armor_id,
                                    shield, hp, atk_bonus, dmg_die, dmg_bonus, ac,
                                    rng, out_dir);
        if (ok) ++success;
    };

    // Ability scores — standardised for simplicity, modified by class minimums
    // B/X has no class ability minimums at basic level, so standard array is fine.

    //  1. Fighter  — Hrothgar
    //     sword, chain mail + shield, 1d8+con hp, atk +1, dmg 1d8, AC 5 + DEX mod
    //     STR 9 → mod 0, DEX 9 → mod 0, CON 9 → mod 0
    {
        Abilities a{13, 9, 11, 9, 9, 10};
        int str_mod = bx::get_strength(a.str).toHitMelee;
        int dex_mod = -bx::get_dexterity(a.dex).acBonus;
        int con_mod = bx::get_constitution(a.con).hpBonusPerDie;
        int hp = roll_hp(8, con_mod, rng);
        gen("Hrothgar", CharacterClass::Fighter, a,
            "sword", "chain-mail", true,
            hp, str_mod, 8, str_mod, 14 + dex_mod);
    }

    //  2. Magic-User  — Merlin
    //     dagger only, no armor, no shield, 1d4+con hp, atk +0, dmg 1d4, AC 10 + DEX
    {
        Abilities a{9, 13, 9, 14, 9, 10};
        int dex_mod = -bx::get_dexterity(a.dex).acBonus;
        int con_mod = bx::get_constitution(a.con).hpBonusPerDie;
        int hp = roll_hp(4, con_mod, rng);
        gen("Merlin", CharacterClass::MagicUser, a,
            "dagger", "none", false,
            hp, 0, 4, 0, 10 + dex_mod);
    }

    //  3. Thief  — Shadow
    //     short-sword, leather, 1d4+con hp, atk +0, dmg 1d6, AC 12 + DEX
    {
        Abilities a{9, 14, 9, 10, 9, 10};
        int dex_mod = -bx::get_dexterity(a.dex).acBonus;
        int con_mod = bx::get_constitution(a.con).hpBonusPerDie;
        int hp = roll_hp(4, con_mod, rng);
        gen("Shadow", CharacterClass::Thief, a,
            "short-sword", "leather-armor", false,
            hp, 0, 6, 0, 12 + dex_mod);
    }

    //  4. Cleric  — Cuthbert
    //     mace, chain mail + shield, 1d6+con hp, atk +0, dmg 1d6, AC 14 + DEX
    {
        Abilities a{9, 9, 11, 9, 14, 13};
        int str_mod = bx::get_strength(a.str).toHitMelee;
        int dex_mod = -bx::get_dexterity(a.dex).acBonus;
        int con_mod = bx::get_constitution(a.con).hpBonusPerDie;
        int hp = roll_hp(6, con_mod, rng);
        gen("Cuthbert", CharacterClass::Cleric, a,
            "mace", "chain-mail", true,
            hp, 0 + str_mod, 6, str_mod, 14 + dex_mod);
    }

    //  5. Dwarf  — Durin
    //     battle-axe, chain mail + shield, 1d8+con hp, atk +1, dmg 1d8, AC 14 + DEX
    {
        Abilities a{13, 9, 12, 9, 9, 10};
        int str_mod = bx::get_strength(a.str).toHitMelee;
        int dex_mod = -bx::get_dexterity(a.dex).acBonus;
        int con_mod = bx::get_constitution(a.con).hpBonusPerDie;
        int hp = roll_hp(8, con_mod, rng);
        gen("Durin", CharacterClass::Dwarf, a,
            "battle-axe", "chain-mail", true,
            hp, 1 + str_mod, 8, str_mod, 14 + dex_mod);
    }

    //  6. Elf  — Aelar
    //     sword, chain mail + shield, 1d6+con hp, atk +1, dmg 1d8, AC 12 + DEX
    {
        Abilities a{13, 11, 9, 12, 9, 10};
        int str_mod = bx::get_strength(a.str).toHitMelee;
        int dex_mod = -bx::get_dexterity(a.dex).acBonus;
        int con_mod = bx::get_constitution(a.con).hpBonusPerDie;
        int hp = roll_hp(6, con_mod, rng);
        gen("Aelar", CharacterClass::Elf, a,
            "sword", "chain-mail", true,
            hp, 1 + str_mod, 8, str_mod, 12 + dex_mod);
    }

    //  7. Halfling  — Bilbo
    //     short-sword, leather, shield, 1d6+con hp, atk +1, dmg 1d6, AC 12 + DEX
    {
        Abilities a{9, 14, 11, 10, 9, 13};
        int str_mod = bx::get_strength(a.str).toHitMelee;
        int dex_mod = -bx::get_dexterity(a.dex).acBonus;
        int con_mod = bx::get_constitution(a.con).hpBonusPerDie;
        int hp = roll_hp(6, con_mod, rng);
        gen("Bilbo", CharacterClass::Halfling, a,
            "short-sword", "leather-armor", true,
            hp, 1 + str_mod, 6, str_mod, 12 + dex_mod);
    }

    std::cout << success << "/" << total << " pregenerated saves created.\n";
    return (success == total) ? 0 : 1;
}
