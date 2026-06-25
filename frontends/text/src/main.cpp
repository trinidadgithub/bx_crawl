#include "game.hpp"

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
    raw.c_oflag |= (OPOST | ONLCR);  // \n → CR+NL on output
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

// Single-key input – no Enter required. Unknown keys are silently ignored.
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
            // Spells that need a target direction
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
        compute_visibility(g);
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

        PlayerCommand cmd = read_player_command(g);
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
