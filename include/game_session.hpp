#pragma once

#include "campaign_state.hpp"
#include "game.hpp"
#include "game_snapshot.hpp"
#include "module_content.hpp"

#include <string>
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

struct ModuleStartResult {
    bool ok{false};
    std::string error;
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

    // Frontend-neutral presentation state for renderers. This is the Phase 3
    // replacement seam for rendering from raw Game internals.
    GameStateSnapshot snapshot() const;

    // Start a data-only module at its manifest entry location. This establishes
    // the first runtime seam for module maps without loading encounters,
    // treasures, traps, or mutable campaign state.
    ModuleStartResult start_module(const ModuleContent& module, Entity hero);

    // Restart a module campaign and re-apply the persisted module runtime
    // state: heard rumors stay heard, previously opened doors restore to open,
    // and the hero resumes at the captured position and location. The module
    // rebuilds its authored map and content from `module`; only mutable state
    // (rumor ids, door flags, position, location) comes from `campaign`.
    ModuleStartResult resume_module(const ModuleContent& module, Entity hero,
                                    const CampaignState& campaign);

    // Structured event stream. Frontends may consume new events by retaining a
    // cursor into this vector; Game still owns event storage for now.
    const std::vector<GameEvent>& events() const;

private:
    // Module-backed runtime state: the declared door routes (stable door id,
    // tile position and both endpoint locations).
    bool module_mode_{false};
    struct ModuleDoorRoute {
        std::string door_id;
        int x{};
        int y{};
        std::string from_location_id;
        std::string to_location_id;
    };
    std::vector<ModuleDoorRoute> module_door_routes_;

    // Refresh Game::module_door_states from the runtime door tiles so
    // campaign capture sees the current open/locked flags.
    void sync_module_door_states();

    void resolve_module_location(const std::string& prev_location);

    Game game_;
};
