#include "game.hpp"

#include <iostream>
#include <cctype>
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
    raw.c_oflag |= (OPOST | ONLCR);  // \n → CR+NL on output
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Single-key input – no Enter required. Unknown keys are silently ignored.
static PlayerCommand read_player_command() {
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

        // Throw weapon: read a second key for direction
        if (c == 'k' || c == 'f') {
            char dir;
            if (read(STDIN_FILENO, &dir, 1) != 1) {
                return PlayerCommand{PlayerCommandType::Wait, 0, 0};
            }
            dir = std::tolower(dir);
            int dx = 0, dy = 0;
            if      (dir == 'w') {              dy = -1; }
            else if (dir == 's') {              dy =  1; }
            else if (dir == 'a') { dx = -1;            }
            else if (dir == 'd') { dx =  1;            }
            else if (dir == 'q') { dx = -1; dy = -1; }
            else if (dir == 'e') { dx =  1;  dy = -1; }
            else if (dir == 'z') { dx = -1; dy =  1; }
            else if (dir == 'c') { dx =  1;  dy =  1; }
            else {
                return PlayerCommand{PlayerCommandType::Wait, 0, 0};
            }
            auto cmd_type = (c == 'k')
                ? PlayerCommandType::ThrowKnife
                : PlayerCommandType::ThrowDart;
            return PlayerCommand{cmd_type, dx, dy};
        }

        // Space / Enter / Return → wait a turn
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

int main() {
    Game g;
    init_game(g);

    // Raw mode for instant keypress AFTER character creation
    set_raw_mode();

    // Clear residual chargen text before entering the dungeon
    std::cout << "\x1B[2J\x1B[H" << std::flush;

    while (true) {
        render(g);

        bool monsters_first = roll_initiative(g);

        if (monsters_first) {
            monsters_turn(g);
            if (!g.hero.alive) {
                render(g);
                std::cout << "You have died.\n";
                std::cout << "Game over.\n";
                break;
            }
        }

        PlayerCommand cmd = read_player_command();
        bool keep_playing = apply_player_command(g, cmd);
        if (!keep_playing) {
            render(g);
            std::cout << "Game over.\n";
            break;
        }

        if (!g.hero.alive) {
            render(g);
            std::cout << "You have died.\n";
            break;
        }

        if (!any_monsters_alive(g)) {
            render(g);
            std::cout << "All monsters defeated!\n";
            std::cout << "Game over.\n";
            break;
        }

        if (!monsters_first) {
            monsters_turn(g);

            if (!g.hero.alive) {
                render(g);
                std::cout << "You have died.\n";
                std::cout << "Game over.\n";
                break;
            }
        }
    }

    return 0;
}
