#include "game_session.hpp"

#include "game.hpp"

void GameSession::prepare_dungeon_frame() {
    compute_visibility(game_);
}

ActionResult GameSession::apply(const PlayerCommand& cmd) {
    Game& g = game_;
    ActionResult result;

    // Non-dungeon flow: single-command delegation into the town handler. This
    // is the counterpart of the former town branch of the text-frontend loop.
    if (g.state != GameState::Dungeon) {
        result.keep_playing = apply_town_command(g, cmd);
        return result;
    }

    // Non-turn commands: applied directly without initiative or monster action,
    // matching the former frontend special-case. Initiative and monster turns
    // therefore only run for commands that pass a dungeon turn.
    if (cmd.type == PlayerCommandType::Inventory ||
        cmd.type == PlayerCommandType::Quit) {
        result.keep_playing = apply_player_command(g, cmd);
        return result;
    }

    const bool monsters_present = any_monsters_alive(g);
    const bool monsters_first = monsters_present && roll_initiative(g);

    if (monsters_first) {
        monsters_turn(g);
        if (!g.hero.alive) {
            result.keep_playing = false;
            result.hero_died = true;
            result.died_before_action = true;
            return result;
        }
    }

    result.keep_playing = apply_player_command(g, cmd);
    if (!result.keep_playing)
        return result;

    // The hero may have left the dungeon (stairs): the turn cycle ends here and
    // dungeon-only checks are skipped.
    if (g.state != GameState::Dungeon)
        return result;

    if (!g.hero.alive) {
        result.keep_playing = false;
        result.hero_died = true;
        return result;
    }

    if (!any_monsters_alive(g))
        announce_dungeon_at_peace(g);

    if (monsters_present && !monsters_first) {
        monsters_turn(g);
        if (!g.hero.alive) {
            result.keep_playing = false;
            result.hero_died = true;
        }
    }

    return result;
}

GameStateSnapshot GameSession::snapshot() const {
    return make_game_state_snapshot(game_);
}

const std::vector<GameEvent>& GameSession::events() const {
    return game_.events;
}
