#include "game.hpp"

#include <iostream>
#include <string>
#include <cctype>

// Map keyboard input to PlayerCommand
static PlayerCommand read_player_command() {
    std::cout << "> ";
    std::string line;
    if (!std::getline(std::cin, line)) {
        // If input is closed, treat as Quit
        return PlayerCommand{PlayerCommandType::Quit, 0, 0};
    }

    if (line.empty()) {
        // Empty line = Wait
        return PlayerCommand{PlayerCommandType::Wait, 0, 0};
    }

    char c = std::tolower(line[0]);
    int dx = 0;
    int dy = 0;

    // x = quit (so q can be a movement key)
    if (c == 'x') {
        return PlayerCommand{PlayerCommandType::Quit, 0, 0};
    }

    // Door commands
    if (c == 'o') {
        return PlayerCommand{PlayerCommandType::Open, 0, 0};
    }
    if (c == 'p') {
        return PlayerCommand{PlayerCommandType::Close, 0, 0};
    }

    // 8-direction movement: qwe/asd/zxc
    if (c == 'w') {           // up
        dy = -1;
    } else if (c == 's') {    // down
        dy = 1;
    } else if (c == 'a') {    // left
        dx = -1;
    } else if (c == 'd') {    // right
        dx = 1;
    } else if (c == 'q') {    // up-left
        dx = -1; dy = -1;
    } else if (c == 'e') {    // up-right
        dx = 1;  dy = -1;
    } else if (c == 'z') {    // down-left
        dx = -1; dy = 1;
    } else if (c == 'c') {    // down-right
        dx = 1;  dy = 1;
    } else {
        // Unknown key -> Wait
        return PlayerCommand{PlayerCommandType::Wait, 0, 0};
    }

    return PlayerCommand{PlayerCommandType::Move, dx, dy};
}

int main() {
    Game g;
    init_game(g);

    while (true) {
        render(g);

        // Roll initiative each round
        bool monsters_first = roll_initiative(g);

        // Monsters act first if they won initiative
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
            // Player quit
            render(g);
            std::cout << "Game over.\n";
            break;
        }

        if (!g.hero.alive) {
            // Hero died from their own action (trap, etc. – later)
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

        // If cin entered a failed state (EOF, pipe close), quit gracefully
        if (!std::cin) {
            std::cout << "Input closed. Goodbye.\n";
            break;
        }

        // Hero won or tied initiative: monsters act after the player
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

