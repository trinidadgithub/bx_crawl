#pragma once

#include "entity.hpp"
#include <string>
#include <vector>

struct Game;  // forward declaration

// ---------------------------------------------------------------------------
// Level-1 character generation rules.
//
// These helpers are non-interactive: they read the Game RNG / bx_data catalog
// and (when asked) mutate an Entity or a purse. Console prompts and UI text
// belong to the frontend that drives them (see frontends/text/char_session).
// ---------------------------------------------------------------------------

// Roll 3d6 for one score, or all six scores (STR/DEX/CON/INT/WIS/CHA).
int roll_3d6(Game& g);
Abilities roll_abilities(Game& g);

// Class menu: classes are offered 1..character_class_count().
int character_class_count();
CharacterClass character_class_at(int index);

// Class rule data for the level-1 sheet and stat derivation.
int hit_die_sides(CharacterClass cc);
int damage_die(CharacterClass cc);
int base_ac(CharacterClass cc);
int attack_bonus_base(CharacterClass cc);
int prime_requisite_score(const Abilities& a, CharacterClass cc);
const char* prime_name(CharacterClass cc);
int xp_bonus(int prime_score);

// Ability modifiers fed into stat derivation.
int strength_to_hit_modifier(const Abilities& a);
int dexterity_ac_modifier(const Abilities& a);
int constitution_hp_modifier(const Abilities& a);

// Random starting resources.
int roll_starting_gold(Game& g); // 3d6 x 10 gp
int roll_starting_hp(Game& g, CharacterClass cc, int con_mod);

// Starting-gear choices. Armor option k >= 1 selects bx::armor_items[k - 1];
// option 0 means "none". Shield buying is a separate step (class_can_use_shield).
std::vector<int> class_weapon_indices(CharacterClass cc);
std::vector<int> class_armor_options(CharacterClass cc);
bool class_can_use_shield(CharacterClass cc);

// Buy starting gear. On success the cost is deducted from purse_gp and the
// hero is equipped; on failure nothing changes and false is returned.
bool buy_starting_weapon(Entity& hero, int bx_weapon_index, int& purse_gp, int str_mod);
bool buy_starting_armor(Entity& hero, int bx_armor_index, int& purse_gp, int dex_mod);
bool buy_starting_shield(Entity& hero, int& purse_gp);

// Starting spell lists (Magic-User/Elf and Cleric; empty for other classes).
const std::vector<std::string>& class_starting_spells(CharacterClass cc);
// Set level-1 slots and known spells. selected_indices are 1-based menu
// choices; select_all grants the full class list (Cleric "all"). Magic-Users
// and Elves always know Read Magic.
void grant_starting_spells(Entity& hero, CharacterClass cc,
                           const std::vector<int>& selected_indices, bool select_all);

// Finish a freshly-built level-1 hero: purse, combat flags, thief skills, and
// ranged ammunition.
void finalize_level_1_hero(Entity& hero, int gold_pieces);