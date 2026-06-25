#pragma once

// Procedural sound effects via WAV generation + aplay/paplay.
// Zero external dependencies — sounds are synthesised at runtime.
// Non-blocking: fork+exec aplay with pipe'd WAV data.

// Initialise random seed for sound synthesis.
void sound_init();

// Master toggle
void sound_set_enabled(bool e);
bool sound_enabled();

// Sound-effect triggers (called from game logic)
void sound_footstep();
void sound_door_open();
void sound_door_close();
void sound_attack_hit();
void sound_monster_death();
void sound_gold_pickup();
void sound_spell();
void sound_trap();
void sound_player_death();
void sound_backstab();
void sound_throw();
