#pragma once

#include "game.hpp"

#include <utility>

// Result of one command cycle through GameSession.
struct ActionResult {
    // False when the caller should stop the main game loop (Quit, death).
    bool keep_playing{true};
    // True when the hero died during this cycle; the caller should present a
    // death frame/message.
    bool hero_died{false};
    // True when the hero died before being able to act this turn (monsters won
    // initiative and killed the hero first). The text frontend shows a
    // "Game over." line in this case to match its historical death text.
    bool died_before_action{false};
};

// Application-layer wrapper around the authoritative Game. GameSession owns the
// gameplay turn cycle (dungeon timing, initiative, monster turns, death and
// peace evaluation) so that frontends submit intent through apply() instead of
// coordinating the turn cycle themselves.
//
// Game remains the authoritative mutable state; GameSession owns a Game by
// value and delegates to the existing engine rules functions.
class GameSession {
public:
    GameSession() = default;
    explicit GameSession(Game game) : game_(std::move(game)) {}

    // Transitional accessors. The text frontend still needs direct access for
    // the startup save/load flow, character generation, and rendering until the
    // future GameStateSnapshot architecture provides a presentation view.
    Game& game() { return game_; }
    const Game& game() const { return game_; }

    // Advance the dungeon's turn clock / hunger / light and recompute the field
    // of view so a fresh dungeon frame can be rendered. This is the time
    // progression half of the dungeon turn (previously invoked directly by the
    // text frontend before each dungeon command).
    void prepare_dungeon_frame();

    // Execute one logical command cycle and return its outcome. In the dungeon
    // this coordinates initiative, monster turns, the player action, death and
    // peace evaluation; outside the dungeon it delegates to the town flow.
    ActionResult apply(const PlayerCommand& cmd);

private:
    Game game_;
};