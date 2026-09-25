#pragma once

#include "game.hpp"

#include <string>

namespace game_internal {

void emit_log_event(Game& g, const std::string& msg);
void emit_sound(Game& g, SoundEffect effect);

bool hero_is_confused(const Game& g);
int daylight_to_hit_penalty(const Game& g, const Entity& m);
int effective_morale(const Game& g, const Entity& m);

void award_treasure(Game& g, Entity& monster);
void check_level_up(Game& g);
bool make_save(Game& g, CharacterClass cc, int category);

void apply_level_drain(Game& g, const Entity& draining);
void apply_strength_drain(Game& g, const Entity& draining);
void apply_rust_corrosion(Game& g, const Entity& rusting);
void apply_green_slime(Game& g, const Entity& slime);
bool hero_weapon_can_wound(const Game& g, WeaponImmunity immunity);
bool elemental_hits_monster(Game& g, Entity& target, int dmg,
                            DamageType damage_type,
                            const std::string& source,
                            std::vector<Entity>& spillover);

int sign(int v);
bool is_occupied(const Game& g, int x, int y, const Entity* ignore = nullptr);
void move_entity(Game& g, Entity& e, int dx, int dy);
Entity* monster_at(Game& g, int x, int y);

int spawn_wandering_monsters(Game& g);
bool resolve_first_contact_surprise(Game& g, Entity& m);
Entity spawn_bx_monster_by_index(Game& g, int idx);
void descend_stairs(Game& g);
void ascend_stairs(Game& g);

// Equipment / inventory rules (defined in inventory.cpp)
int parse_die_size(const char* s);
bool can_use_shield(CharacterClass cc);
bool can_wear_heavy_armor(CharacterClass cc);
void equip_owned_weapon(Game& g, int inventory_index);
void equip_owned_armor(Game& g, int armor_index);

} // namespace game_internal

void attack(Game& g, Entity& attacker, Entity& defender);
void try_morale_check(Game& g, Entity& m, Entity& attacker,
                      int& free_attacks_remaining, const std::string& reason);
void flee_ai(Game& g, Entity& m);
bool cast_spell(Game& g, int spell_index, int dx, int dy);

// Exploration / time helpers (defined in exploration.cpp)
void tick_wander_counter(Game& g);
bool hero_search(Game& g);
bool hero_move(Game& g, int dx, int dy);
