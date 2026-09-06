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

int sign(int v);
bool is_occupied(const Game& g, int x, int y, const Entity* ignore = nullptr);
void move_entity(Game& g, Entity& e, int dx, int dy);

} // namespace game_internal

void attack(Game& g, Entity& attacker, Entity& defender);
void try_morale_check(Game& g, Entity& m, Entity& attacker,
                      int& free_attacks_remaining, const std::string& reason);
void flee_ai(Game& g, Entity& m);
