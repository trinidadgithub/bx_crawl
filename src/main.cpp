#include "game.hpp"
#include <iostream>

int main() {
    Game g;
    init_game(g);

    bool running = true;
    while (running) {
        render(g);

        bool monsters_alive = any_monsters_alive(g);

        if (!g.hero.alive || !monsters_alive) {
            std::cout << "\nGame over.\n";
            if (!g.hero.alive && !monsters_alive) {
                std::cout << "You died, but took all the monsters with you.\n";
            } else if (!g.hero.alive) {
                std::cout << "You have been slain.\n";
            } else {
                std::cout << "You have slain all the monsters!\n";
            }
            break;
        }

        running = handle_player_input(g);
        if (!running) break;

        if (g.hero.alive && any_monsters_alive(g)) {
            monsters_turn(g);
        }
    }

    std::cout << "Thanks for playing.\n";
    return 0;
}

