#include "game.hpp"
#include "bx_data.hpp"
#include "sound.hpp"

#include <iostream>
#include <cctype>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <cstdlib>

static termios original_termios;
static bool termios_saved = false;

static void restore_terminal() {
    if (termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
    }
}

static void set_raw_mode() {
    tcgetattr(STDIN_FILENO, &original_termios);
    termios_saved = true;
    std::atexit(restore_terminal);
    termios raw = original_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_oflag |= (OPOST | ONLCR);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Helper: read a direction key (wqe/asd/zxc), store dx/dy, return true on match
static bool read_direction(int& dx, int& dy) {
    char dir;
    if (read(STDIN_FILENO, &dir, 1) != 1) return false;
    dir = std::tolower(dir);
    if      (dir == 'w') {              dy = -1; }
    else if (dir == 's') {              dy =  1; }
    else if (dir == 'a') { dx = -1;            }
    else if (dir == 'd') { dx =  1;            }
    else if (dir == 'q') { dx = -1; dy = -1; }
    else if (dir == 'e') { dx =  1;  dy = -1; }
    else if (dir == 'z') { dx = -1; dy =  1; }
    else if (dir == 'c') { dx =  1;  dy =  1; }
    else return false;
    return true;
}

static PlayerCommand read_player_command(const Game& g) {
    for (;;) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) {
            return PlayerCommand{PlayerCommandType::Quit, 0, 0};
        }
        c = std::tolower(c);

        if (c == 'x') {
            std::cout << "Really quit? (y/n) " << std::flush;
            char reply;
            if (read(STDIN_FILENO, &reply, 1) != 1) {
                return PlayerCommand{PlayerCommandType::Quit, 0, 0};
            }
            if (reply == 'y' || reply == 'Y') {
                return PlayerCommand{PlayerCommandType::Quit, 0, 0};
            }
            continue;
        }
        if (c == 'o') {
            return PlayerCommand{PlayerCommandType::Open, 0, 0};
        }
        if (c == 'p') {
            return PlayerCommand{PlayerCommandType::Close, 0, 0};
        }
        if (c == 'r') {
            return PlayerCommand{PlayerCommandType::Rest, 0, 0};
        }
        if (c == 'g') {
            return PlayerCommand{PlayerCommandType::Get, 0, 0};
        }

        if (c == 'i') {
            return PlayerCommand{PlayerCommandType::Hide, 0, 0};
        }

        if (c == 'l') {
            return PlayerCommand{PlayerCommandType::Search, 0, 0};
        }

        if (c == 'u') {
            return PlayerCommand{PlayerCommandType::Use, 0, 0};
        }

        if (c == 'h') {
            const char* help =
                "\x1B[2J\x1B[H"
                "=== Help ===\r\n"
                "\r\n"
                "Movement:  w e   (7 8 9)\r\n"
                "           a d   (4 5 6)\r\n"
                "           z c   (1 2 3)\r\n"
                "\r\n"
                "o  Open a door\r\n"
                "p  Close a door\r\n"
                "r  Rest (heal 1d3/level)\r\n"
                "k  Throw knife (direction)\r\n"
                "f  Throw dart  (direction)\r\n"
                "g  Get items from the ground\r\n"
                "i  Hide in shadows (Thief)\r\n"
                "l  Search (find traps, hear noise)\r\n"
                "u  Use / read a scroll\r\n"
                "m  Cast a spell\r\n"
                "t  Turn undead (direction)\r\n"
                "x  Quit\r\n"
                "\r\n"
                "Press any key to return...\r\n";
            write(STDOUT_FILENO, help, strlen(help));
            char _;
            read(STDIN_FILENO, &_, 1);
            continue;
        }

        // Magic: show known spells, pick one, then direction if needed
        if (c == 'm') {
            const auto& spells = g.hero.spells;
            if (spells.empty()) {
                std::cout << "You know no spells.\r\n" << std::flush;
                continue;
            }
            std::cout << "Cast which spell?\r\n";
            for (std::size_t i = 0; i < spells.size(); ++i) {
                std::cout << "  " << (i + 1) << ". " << spells[i] << "\r\n";
            }
            std::cout << "> " << std::flush;
            char num;
            if (read(STDIN_FILENO, &num, 1) != 1)
                return PlayerCommand{PlayerCommandType::Wait, 0, 0};
            int idx = num - '1';
            if (idx < 0 || idx >= (int)spells.size())
                continue;
            int dx = 0, dy = 0;
            const std::string& spell = spells[idx];
            if (spell == "Magic Missile" || spell == "Charm Person") {
                std::cout << "In which direction? " << std::flush;
                if (!read_direction(dx, dy))
                    continue;
            }
            return PlayerCommand{PlayerCommandType::Cast, dx, dy, idx};
        }

        // Turn undead (Cleric): read a direction
        if (c == 't') {
            int dx = 0, dy = 0;
            if (!read_direction(dx, dy))
                continue;
            return PlayerCommand{PlayerCommandType::Turn, dx, dy, 0};
        }

        // Throw weapon: read a second key for direction
        if (c == 'k' || c == 'f') {
            int dx = 0, dy = 0;
            if (!read_direction(dx, dy))
                return PlayerCommand{PlayerCommandType::Wait, 0, 0};
            auto cmd_type = (c == 'k')
                ? PlayerCommandType::ThrowKnife
                : PlayerCommandType::ThrowDart;
            return PlayerCommand{cmd_type, dx, dy, 0};
        }

        // Space / Enter / Return -> wait a turn
        if (c == ' ' || c == '\n' || c == '\r') {
            return PlayerCommand{PlayerCommandType::Wait, 0, 0};
        }

        int dx = 0, dy = 0;
        if      (c == 'w') {              dy = -1; }
        else if (c == 's') {              dy =  1; }
        else if (c == 'a') { dx = -1;            }
        else if (c == 'd') { dx =  1;            }
        else if (c == 'q') { dx = -1; dy = -1; }
        else if (c == 'e') { dx =  1;  dy = -1; }
        else if (c == 'z') { dx = -1; dy =  1; }
        else if (c == 'c') { dx =  1;  dy =  1; }
        else {
            continue; // unknown key – ignore
        }

        return PlayerCommand{PlayerCommandType::Move, dx, dy};
    }
}

// ── Town input: map single-key presses to commands ───────
static PlayerCommand read_town_command(const Game& g) {
    for (;;) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) {
            return PlayerCommand{PlayerCommandType::Quit, 0, 0};
        }
        c = std::tolower(c);

        if (c == 'x') {
            std::cout << "Really quit? (y/n) " << std::flush;
            char reply;
            if (read(STDIN_FILENO, &reply, 1) != 1) continue;
            if (reply == 'y' || reply == 'Y')
                return PlayerCommand{PlayerCommandType::Quit, 0, 0};
            continue;
        }

        // Map keys to locations / actions
        switch (g.state) {
        case GameState::Town:
            if (c == '1') return PlayerCommand{PlayerCommandType::Move,   0, -1}; // tavern
            if (c == '2') return PlayerCommand{PlayerCommandType::Move,  -1,  0}; // temple
            if (c == '3') return PlayerCommand{PlayerCommandType::Move,   1,  0}; // shop
            if (c == '4') return PlayerCommand{PlayerCommandType::Move,  -1,  1}; // armorer
            if (c == '5') return PlayerCommand{PlayerCommandType::Move,   0,  1}; // camp
            break;

        case GameState::Camp:
            if (c == '1') return PlayerCommand{PlayerCommandType::Rest, 0, 0};
            if (c == '2') return PlayerCommand{PlayerCommandType::Help, 0, 0}; // save
            if (c == '3') return PlayerCommand{PlayerCommandType::Move, 0, -1}; // enter dungeon
            if (c == 't') return PlayerCommand{PlayerCommandType::Move, 0,  1}; // return to town
            break;

        case GameState::Tavern:
            if (c == '1') return PlayerCommand{PlayerCommandType::Rest, 0, 0};
            if (c == '2') return PlayerCommand{PlayerCommandType::Help, 0, 0}; // rumors
            if (c == 't') return PlayerCommand{PlayerCommandType::Move, 2, 0}; // return to town
            if (c == 'c') return PlayerCommand{PlayerCommandType::Move, 0, 1};
            break;

        case GameState::Temple:
            if (c == '1') return PlayerCommand{PlayerCommandType::Cast, 0, 0}; // healing
            if (c == '2') return PlayerCommand{PlayerCommandType::Turn, 0, 0}; // donate
            if (c == 't') return PlayerCommand{PlayerCommandType::Move, 2, 0};
            if (c == 'c') return PlayerCommand{PlayerCommandType::Move, 0, 1};
            break;

        case GameState::Shop:
            if (g.town_submenu == 0) {
                if (c == '1') return PlayerCommand{PlayerCommandType::Get, 0, 0}; // buy torch
                if (c == '2') return PlayerCommand{PlayerCommandType::Use, 0, 0}; // supplies submenu
            } else if (g.town_submenu == 4) {
                if (c >= '1' && c <= '4')
                    return PlayerCommand{PlayerCommandType::Get, 0, 0, c - '1'};
                if (c == 't') return PlayerCommand{PlayerCommandType::Help, 0, 0};
            }
            if (c == 't') return PlayerCommand{PlayerCommandType::Move, 2, 0};
            if (c == 'c') return PlayerCommand{PlayerCommandType::Move, 0, 1};
            break;

        case GameState::Armorer:
            if (g.town_submenu == 0) {
                if (c == '1') return PlayerCommand{PlayerCommandType::Get, 0, 0};
                if (c == '2') return PlayerCommand{PlayerCommandType::Get, 0, 0};
                if (c == '3') return PlayerCommand{PlayerCommandType::Get, 0, 0};
            } else if (g.town_submenu == 1) {
                int allowed = 0;
                for (const auto& w : bx::weapons)
                    if (is_weapon_allowed(g.hero.char_class, w.id)) ++allowed;
                if (c >= '1' && c <= '0' + allowed)
                    return PlayerCommand{PlayerCommandType::Get, 0, 0, c - '1'};
                if (c == 't') return PlayerCommand{PlayerCommandType::Help, 0, 0};
            } else if (g.town_submenu == 2) {
                if (c >= '1' && c <= '4')
                    return PlayerCommand{PlayerCommandType::Get, 0, 0, c - '1'};
                if (c == 't') return PlayerCommand{PlayerCommandType::Help, 0, 0};
            } else if (g.town_submenu == 3) {
                if (c >= '1' && c <= '3')
                    return PlayerCommand{PlayerCommandType::Get, 0, 0, c - '1'};
                if (c == 't') return PlayerCommand{PlayerCommandType::Help, 0, 0};
            }
            if (c == 't') return PlayerCommand{PlayerCommandType::Move, 2, 0};
            if (c == 'c') return PlayerCommand{PlayerCommandType::Move, 0, 1};
            break;

        default:
            break;
        }

        // If nothing matched, ignore the key (don't loop back to render
        // — the caller will re-render after this returns with the same state)
        continue;
    }
}

int main() {
    Game g;
    init_game(g);
    sound_set_enabled(true);

    // Raw mode for instant keypress AFTER character creation
    set_raw_mode();

    // Clear residual chargen text
    std::cout << "\x1B[2J\x1B[H" << std::flush;

    while (true) {
        // ── Render ──────────────────────────────────────
        render(g);

        // ── Command input & dispatch ─────────────────────
        bool keep_playing = true;

        if (g.state == GameState::Dungeon) {
            // ── Dungeon loop ────────────────────────────
            compute_visibility(g);
            render(g);

            bool monsters_first = roll_initiative(g);

            if (monsters_first) {
                monsters_turn(g);
                if (!g.hero.alive) {
                    render(g);
                    std::cout << "You have died.\r\n";
                    std::cout << "Game over.\r\n";
                    break;
                }
            }

            {
                PlayerCommand cmd = read_player_command(g);
                keep_playing = apply_player_command(g, cmd);
            }

            if (!keep_playing) break;

            // State may have changed to Camp via ascend_stairs — skip dungeon-only checks
            if (g.state != GameState::Dungeon) continue;

            if (!g.hero.alive) {
                render(g);
                std::cout << "You have died.\r\n";
                break;
            }

            if (!any_monsters_alive(g)) {
                render(g);
                std::cout << "All monsters defeated!\r\n";
                std::cout << "Game over.\r\n";
                break;
            }

            if (!monsters_first) {
                monsters_turn(g);
                if (!g.hero.alive) {
                    render(g);
                    std::cout << "You have died.\r\n";
                    break;
                }
            }
        } else {
            // ── Town loop ───────────────────────────────
            PlayerCommand cmd = read_town_command(g);
            keep_playing = apply_town_command(g, cmd);
            if (!keep_playing) break;
        }
    }

    return 0;
}
