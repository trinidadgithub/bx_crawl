#pragma once

#include <string>

enum class SoundEffect {
    Footstep,
    DoorOpen,
    DoorClose,
    AttackHit,
    MonsterDeath,
    GoldPickup,
    Spell,
    Trap,
    PlayerDeath,
    Backstab,
    Throw
};

enum class GameEventType {
    LogMessage,
    Sound
};

struct GameEvent {
    GameEventType type{GameEventType::LogMessage};
    std::string text;
    SoundEffect sound{SoundEffect::Footstep};
};
