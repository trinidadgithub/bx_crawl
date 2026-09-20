#include "game.hpp"
#include "game_session.hpp"
#include "render_snapshot.hpp"
#include "char_session.hpp"
#include "bx_data.hpp"
#include "sound.hpp"
#include "module_content.hpp"
#include "module_manifest.hpp"

#include <iostream>
#include <cctype>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>

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
            return PlayerCommand{PlayerCommandType::Quit, 0, 0};
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

        if (c == 's') {
            return PlayerCommand{PlayerCommandType::Scrape, 0, 0};
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

        if (c == 'b') {
            return PlayerCommand{PlayerCommandType::Inventory, 0, 0};
        }

        if (c == 'y') {
            return PlayerCommand{PlayerCommandType::Save, 0, 0};
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
                "v  Throw holy water (direction)\r\n"
                "n  Throw burning oil (direction)\r\n"
                "g  Get items from the ground\r\n"
                "s  Scrape off clinging green slime (costs a round)\r\n"
                "b  Character sheet / equipment\r\n"
                "i  Hide in shadows (Thief)\r\n"
                "l  Search (find traps, hear noise)\r\n"
                "u  Use (light lantern/torch, spike doors, read scrolls)\r\n"
                "m  Cast a spell\r\n"
                "t  Turn undead (direction)\r\n"
                "y  Save game\r\n"
                "x  Quit\r\n"
                "\r\n"
                "Press any key to return...\r\n";
            write(STDOUT_FILENO, help, strlen(help));
            char _;
            read(STDIN_FILENO, &_, 1);
            return PlayerCommand{PlayerCommandType::None, 0, 0};
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
        if (c == 'k' || c == 'f' || c == 'v' || c == 'n') {
            int dx = 0, dy = 0;
            if (!read_direction(dx, dy))
                return PlayerCommand{PlayerCommandType::Wait, 0, 0};
            auto cmd_type = PlayerCommandType::ThrowHolyWater;
            if (c == 'k') cmd_type = PlayerCommandType::ThrowKnife;
            else if (c == 'f') cmd_type = PlayerCommandType::ThrowDart;
            else if (c == 'n') cmd_type = PlayerCommandType::ThrowOil;
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
            return PlayerCommand{PlayerCommandType::Quit, 0, 0};
        }

        if (c == 'b') {
            return PlayerCommand{PlayerCommandType::Inventory, 0, 0};
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
                "v  Throw holy water (direction)\r\n"
                "n  Throw burning oil (direction)\r\n"
                "g  Get items from the ground\r\n"
                "b  Character sheet / equipment\r\n"
                "i  Hide in shadows (Thief)\r\n"
                "l  Search (find traps, hear noise)\r\n"
                "u  Use (light lantern/torch, spike doors, read scrolls)\r\n"
                "m  Cast a spell\r\n"
                "t  Turn undead (direction)\r\n"
                "x  Quit\r\n"
                "\r\n"
                "Press any key to return...\r\n";
            write(STDOUT_FILENO, help, strlen(help));
            char _;
            read(STDIN_FILENO, &_, 1);
            return PlayerCommand{PlayerCommandType::None, 0, 0};
        }

        // Map keys to locations / actions
        switch (g.state) {
        case GameState::CharacterSheet:
            if (g.town_submenu == 0) {
                if (c == '1' || c == '2')
                    return PlayerCommand{PlayerCommandType::Get, 0, 0, c - '1'};
            } else if (g.town_submenu == 6) {
                if (c >= '1' && c <= '9')
                    return PlayerCommand{PlayerCommandType::Get, 0, 0, c - '1'};
            } else if (g.town_submenu == 7) {
                if (c >= '0' && c <= '4')
                    return PlayerCommand{PlayerCommandType::Get, 0, 0, c - '1'};
            }
            if (c == 't') return PlayerCommand{PlayerCommandType::Help, 0, 0};
            break;

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
            if (c == '2') return PlayerCommand{PlayerCommandType::Listen, 0, 0}; // rumors
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
                if (c == '1') return PlayerCommand{PlayerCommandType::Use, 0, 0}; // browse gear
            } else if (g.town_submenu == 4 || g.town_submenu == 5) {
                int base = (g.town_submenu == 4) ? 0 : 10;
                if (c >= '1' && c <= '9')
                    return PlayerCommand{PlayerCommandType::Get, 0, 0, base + (c - '1')};
                if (c == '0')
                    return PlayerCommand{PlayerCommandType::Get, 0, 0, base + 9};
                if (c == 'n' && g.town_submenu == 4) return PlayerCommand{PlayerCommandType::Open, 0, 0};
                if (c == 'p' && g.town_submenu == 5) return PlayerCommand{PlayerCommandType::Close, 0, 0};
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

static void play_new_sound_events(const GameSession& session, std::size_t& cursor) {
    const auto& events = session.events();
    if (cursor > events.size())
        cursor = events.size();
    while (cursor < events.size()) {
        const GameEvent& event = events[cursor++];
        if (event.type == GameEventType::Sound)
            sound_play(event.sound);
    }
}

// Re-enter a freshly loaded game: a module campaign save is bound back through
// resume_module (which rebuilds the authored map and restores heard rumors,
// open doors, position, and location), while a procedural save settles any
// dungeon at camp. Returns false when the load must be treated as unusable.
static bool reenter_loaded_save(GameSession& session,
                                const std::filesystem::path& data_dir,
                                const std::string& label) {
    Game& g = session.game();
    if (!g.module_id.empty()) {
        auto module_dir = find_module_dir(data_dir, g.module_id);
        if (module_dir.empty())
            return false;
        auto content = load_module_content(module_dir);
        if (!content.ok)
            return false;
        CampaignState campaign = capture_campaign_state(g);
        ModuleStartResult started =
            session.resume_module(content.module, campaign.hero, campaign);
        if (!started.ok)
            return false;
        Game& resumed = session.game();
        resumed.event_log.clear();
        resumed.event_log.push_back("Loaded " + label + ".");
        return true;
    }
    if (g.state == GameState::Dungeon)
        g.state = GameState::Camp;
    g.event_log.clear();
    g.event_log.push_back("Loaded " + label + ".");
    return true;
}

int main() {
    // Resolve data_dir relative to the binary so it works from any CWD
    std::filesystem::path bx_data_dir = "../data";
    {
        std::error_code ec;
        auto bin_path = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec) {
            // binary at build/frontends/text/bx_crawl_text → data/ is ../../../data
            auto d = bin_path.parent_path().parent_path().parent_path().parent_path() / "data";
            if (std::filesystem::exists(d / "pregenerated_saves", ec))
                bx_data_dir = std::filesystem::weakly_canonical(d, ec);
        }
        if (ec || !std::filesystem::exists(bx_data_dir / "pregenerated_saves", ec)) {
            // fallback: just use the old default
            bx_data_dir = "../data";
        }
    }

    // One GameSession owns the authoritative Game. The frontend keeps access to
    // the Game (via the transitional accessor) for rendering and the startup
    // save/load flow, and submits gameplay intent through GameSession::apply.
    GameSession session;
    Game& g = session.game();

    // Check BX_LOAD_SAVE env var for auto-load (testing / speed)
    bool loaded = false;
    const char* quick_load = std::getenv("BX_LOAD_SAVE");
    if (quick_load && quick_load[0]) {
        auto path = find_save(quick_load, bx_data_dir);
        if (!path.empty()) {
            init_game_runtime(g, bx_data_dir);
            loaded = load_game(g, path);
            if (loaded)
                loaded = reenter_loaded_save(session, bx_data_dir, quick_load);
        }
    }

    if (!loaded) {
        // List all available saves (user + pregenerated)
        auto saves = enumerate_all_saves(bx_data_dir);
        if (!saves.empty()) {
            std::cout << "=== Saved Characters ===\r\n\r\n";
            for (std::size_t i = 0; i < saves.size(); ++i) {
                bool is_pregen = saves[i].string().find("pregenerated") != std::string::npos;
                std::cout << "  [" << (i + 1) << "] " << strip_save_extension(saves[i]);
                if (is_pregen) std::cout << " (template)";
                std::cout << "\r\n";
            }
            std::cout << "\r\nEnter number to load, or 0 for a new game:\r\n> " << std::flush;
            std::string reply;
            std::getline(std::cin, reply);
            int choice = 0;
            if (!reply.empty()) choice = std::atoi(reply.c_str());
            if (choice >= 1 && choice <= static_cast<int>(saves.size())) {
                init_game_runtime(g, bx_data_dir);
                loaded = load_game(g, saves[static_cast<std::size_t>(choice - 1)]);
                if (loaded)
                    loaded = reenter_loaded_save(
                        session, bx_data_dir,
                        strip_save_extension(saves[static_cast<std::size_t>(choice - 1)]));
                if (!loaded)
                    std::cout << "Failed to load save. Starting a new game.\n";
            }
        }
    }

    // Legacy fallback: old ~/.bx_crawl/save.dat (single-file format)
    if (!loaded) {
        const char* home = std::getenv("HOME");
        auto old_path = home && *home
            ? std::filesystem::path(home) / ".bx_crawl" / "save.dat"
            : std::filesystem::path("bx_crawl_save.dat");
        if (std::filesystem::exists(old_path)) {
            std::cout << "Old save found at " << old_path << ". Load it? (y/n): " << std::flush;
            std::string reply;
            std::getline(std::cin, reply);
            if (!reply.empty() && (reply[0] == 'y' || reply[0] == 'Y')) {
                init_game_runtime(g, bx_data_dir);
                loaded = load_game(g, old_path);
                if (loaded) {
                    loaded = reenter_loaded_save(session, bx_data_dir, "legacy save");
                    if (loaded && session.game().module_id.empty())
                        session.game().event_log.push_back(
                            "Re-save at camp to migrate to per-character format.");
                } else {
                    std::cout << "Failed to load legacy save.\n";
                }
            }
        }
    }

    if (!loaded) {
        // Seed the RNG and prepare fresh runtime state first, so character
        // creation rolls and the generated dungeon share one seed stream.
        init_game_runtime(g, bx_data_dir);
        Entity hero = create_hero_interactive(g);
        start_game_with_hero(g, std::move(hero), bx_data_dir);
    }

    sound_init();
    sound_set_enabled(true);
    std::size_t sound_event_cursor = session.events().size();

    // Raw mode for instant keypress AFTER character creation
    set_raw_mode();

    // Clear residual chargen text
    std::cout << "\x1B[2J\x1B[H" << std::flush;

    while (true) {
        // ── Render a fresh frame ──────────────────────────
        // Dungeon frames advance dungeon timing/visibility first (GameSession
        // owns that step); town frames render directly.
        if (g.state == GameState::Dungeon) {
            session.prepare_dungeon_frame();
            play_new_sound_events(session, sound_event_cursor);
        }
        render(session.snapshot());

        // ── Command input ─────────────────────────────────
        PlayerCommand cmd = (g.state == GameState::Dungeon)
            ? read_player_command(g)
            : read_town_command(g);

        // ── GameSession owns the gameplay turn cycle ──────
        // Initiative, monster turns, the player action, death checks, peace
        // announcement, and state-transition handling all happen inside
        // apply(). The frontend only submits intent and presents the result.
        ActionResult result = session.apply(cmd);
        play_new_sound_events(session, sound_event_cursor);

        if (result.hero_died) {
            render(session.snapshot());
            std::cout << "You have died.\r\n";
            if (result.died_before_action)
                std::cout << "Game over.\r\n";
        }

        if (!result.keep_playing)
            break;
    }

    return 0;
}
