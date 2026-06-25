#include "sound.hpp"
#include "debug_log.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <csignal>
#include <vector>

#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static bool g_enabled = false; // disabled by default (e.g. in tests)

// Simple rate limiter: skip if this sound was played less than N ms ago
#include <chrono>
static bool rate_limited(const char* name, int min_ms = 80) {
    using namespace std::chrono;
    static auto last = steady_clock::now();
    auto now = steady_clock::now();
    auto elapsed = duration_cast<milliseconds>(now - last).count();
    if (elapsed < min_ms) {
        debug_log(std::string("[sound] RATE-LIMITED ") + name + " (" + std::to_string(elapsed) + "ms)");
        return true;
    }
    last = now;
    return false;
}

// ── helpers ──────────────────────────────────────────────

void sound_init() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    debug_log_set_enabled(true);
    debug_log("[sound] sound_init");
    // Auto-reap child processes (aplay instances) so they don't become zombies
    // NOLINTNEXTLINE
    signal(SIGCHLD, SIG_IGN);
}

void sound_set_enabled(bool e) {
    g_enabled = e;
    debug_log(std::string("[sound] set_enabled=") + (e ? "true" : "false"));
}
bool sound_enabled() { return g_enabled; }

// Write a 16-bit mono WAV byte buffer from raw samples.
static std::vector<uint8_t> make_wav(const std::vector<int16_t>& samples,
                                     int sample_rate = 22050) {
    int data_size  = static_cast<int>(samples.size()) * 2;
    int file_size  = 36 + data_size;
    int byte_rate  = sample_rate * 2;

    std::vector<uint8_t> wav;
    wav.reserve(static_cast<std::size_t>(44 + data_size));

    auto put32 = [&](uint32_t v) {
        wav.push_back(static_cast<uint8_t>(v & 0xff));
        wav.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
        wav.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
        wav.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
    };
    auto put16 = [&](uint16_t v) {
        wav.push_back(static_cast<uint8_t>(v & 0xff));
        wav.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
    };

    // RIFF
    wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
    put32(static_cast<uint32_t>(file_size));
    wav.insert(wav.end(), {'W', 'A', 'V', 'E'});

    // fmt
    wav.insert(wav.end(), {'f', 'm', 't', ' '});
    put32(16);          // subchunk size
    put16(1);           // PCM
    put16(1);           // mono
    put32(static_cast<uint32_t>(sample_rate));
    put32(static_cast<uint32_t>(byte_rate));
    put16(2);           // block align
    put16(16);          // bits per sample

    // data
    wav.insert(wav.end(), {'d', 'a', 't', 'a'});
    put32(static_cast<uint32_t>(data_size));

    for (auto s : samples) {
        put16(static_cast<uint16_t>(s));
    }

    return wav;
}

// Fork+exec aplay to play WAV bytes via stdin pipe.
static void play_wav(const std::vector<uint8_t>& wav_data) {
    if (!g_enabled || wav_data.empty()) {
        debug_log("[sound] play_wav skipped (disabled or empty)");
        return;
    }

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        debug_log("[sound] pipe() failed");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // ── child ──
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        // Silence stdout/stderr
        int fd = open("/dev/null", O_WRONLY);
        if (fd >= 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        // Try aplay, then paplay
        execlp("aplay",  "aplay",  "-q", "-", nullptr);
        execlp("paplay", "paplay", "-",   nullptr);
        debug_log("[sound] neither aplay nor paplay found, exiting");
        _exit(1);
    } else if (pid > 0) {
        // ── parent ──
        close(pipefd[0]);
        // NOLINTNEXTLINE
        ssize_t written = write(pipefd[1], wav_data.data(), wav_data.size());
        close(pipefd[1]); // EOF signals aplay to finish
        debug_log("[sound] forked pid=" + std::to_string(pid)
                  + " wrote=" + std::to_string(written) + "B");
    } else {
        debug_log("[sound] fork() failed");
        close(pipefd[0]);
        close(pipefd[1]);
    }
}

// ── noise generators ─────────────────────────────────────

static double white_noise() {
    return static_cast<double>(std::rand()) / RAND_MAX * 2.0 - 1.0;
}

// ── public sound effects ─────────────────────────────────

void sound_footstep() {
    if (rate_limited("footstep", 100)) return;
    debug_log("[sound] footstep");
    // 40 ms quiet noise burst with fast decay
    int n = 40 * 22050 / 1000; // ~882
    std::vector<int16_t> s(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        double env = 1.0 - static_cast<double>(i) / n;
        s[static_cast<std::size_t>(i)] = static_cast<int16_t>(white_noise() * 600.0 * env);
    }
    play_wav(make_wav(s));
}

void sound_door_open() {
    debug_log("[sound] door_open");
    // Low creak — 150 ms low-frequency noise sweep
    int n = 150 * 22050 / 1000;
    std::vector<int16_t> s(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        double t  = static_cast<double>(i) / 22050.0;
        double f  = 80.0 + static_cast<double>(i) / n * 30.0; // 80→110 Hz
        double env = 1.0 - static_cast<double>(i) / n;
        double sig = std::sin(2.0 * M_PI * f * t) * 0.5 + white_noise() * 0.5;
        s[static_cast<std::size_t>(i)] = static_cast<int16_t>(sig * 4000.0 * env);
    }
    play_wav(make_wav(s));
}

void sound_door_close() {
    debug_log("[sound] door_close");
    // Short thud — 80 ms low thump
    int n = 80 * 22050 / 1000;
    std::vector<int16_t> s(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        double env = 1.0 - static_cast<double>(i) / n;
        double sig = white_noise() * 0.7 + std::sin(2.0 * M_PI * 60.0 * i / 22050.0) * 0.3;
        s[static_cast<std::size_t>(i)] = static_cast<int16_t>(sig * 5000.0 * env);
    }
    play_wav(make_wav(s));
}

void sound_attack_hit() {
    if (rate_limited("attack_hit", 100)) return;
    debug_log("[sound] attack_hit");
    // 80 ms sharp impact — noise burst + low thump
    int n = 80 * 22050 / 1000;
    std::vector<int16_t> s(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        double env = 1.0 - static_cast<double>(i) / n;
        double env2 = env * env; // sharper decay
        double sig = white_noise() * 0.6 + std::sin(2.0 * M_PI * 120.0 * i / 22050.0) * 0.4;
        s[static_cast<std::size_t>(i)] = static_cast<int16_t>(sig * 8000.0 * env2);
    }
    play_wav(make_wav(s));
}

void sound_monster_death() {
    debug_log("[sound] monster_death");
    // 300 ms descending moan
    int n = 300 * 22050 / 1000;
    std::vector<int16_t> s(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        double t   = static_cast<double>(i) / 22050.0;
        double f   = 200.0 - static_cast<double>(i) / n * 150.0;
        double env = 1.0 - static_cast<double>(i) / n;
        double sig = std::sin(2.0 * M_PI * f * t) + white_noise() * 0.3;
        s[static_cast<std::size_t>(i)] = static_cast<int16_t>(sig * 5000.0 * env);
    }
    play_wav(make_wav(s));
}

void sound_gold_pickup() {
    if (rate_limited("gold_pickup", 100)) return;
    debug_log("[sound] gold_pickup");
    // 120 ms bright ding — two sine tones
    int n = 120 * 22050 / 1000;
    std::vector<int16_t> s(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        double t   = static_cast<double>(i) / 22050.0;
        double env = 1.0 - static_cast<double>(i) / n;
        double sig = std::sin(2.0 * M_PI * 880.0 * t) * 0.6
                   + std::sin(2.0 * M_PI * 1320.0 * t) * 0.4;
        s[static_cast<std::size_t>(i)] = static_cast<int16_t>(sig * 6000.0 * env);
    }
    play_wav(make_wav(s));
}

void sound_spell() {
    if (rate_limited("spell", 100)) return;
    debug_log("[sound] spell");
    // 350 ms — rising frequency sweep with harmonics
    int n = 350 * 22050 / 1000;
    std::vector<int16_t> s(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        double t   = static_cast<double>(i) / 22050.0;
        double f   = 200.0 + static_cast<double>(i) / n * 600.0;
        double env = 1.0 - static_cast<double>(i) / n;
        double sig = std::sin(2.0 * M_PI * f * t) * 0.5
                   + std::sin(2.0 * M_PI * f * 1.5 * t) * 0.3
                   + white_noise() * 0.2;
        s[static_cast<std::size_t>(i)] = static_cast<int16_t>(sig * 5000.0 * env);
    }
    play_wav(make_wav(s));
}

void sound_trap() {
    debug_log("[sound] trap");
    // 400 ms — pulsing alarm-like tone
    int n = 400 * 22050 / 1000;
    std::vector<int16_t> s(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        double t   = static_cast<double>(i) / 22050.0;
        double pulse = 0.5 + 0.5 * std::sin(2.0 * M_PI * 12.0 * t); // 12 Hz tremolo
        double env = 1.0 - static_cast<double>(i) / n;
        double sig = (std::sin(2.0 * M_PI * 400.0 * t) * 0.6 + white_noise() * 0.4) * pulse;
        s[static_cast<std::size_t>(i)] = static_cast<int16_t>(sig * 6000.0 * env);
    }
    play_wav(make_wav(s));
}

void sound_player_death() {
    debug_log("[sound] player_death");
    // 600 ms — descending sweep to low rumble
    int n = 600 * 22050 / 1000;
    std::vector<int16_t> s(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        double t   = static_cast<double>(i) / 22050.0;
        double f   = 300.0 - static_cast<double>(i) / n * 250.0;
        double env = 1.0 - static_cast<double>(i) / n;
        double sig = std::sin(2.0 * M_PI * f * t) * 0.5 + white_noise() * 0.5;
        s[static_cast<std::size_t>(i)] = static_cast<int16_t>(sig * 7000.0 * env);
    }
    play_wav(make_wav(s));
}

void sound_backstab() {
    debug_log("[sound] backstab");
    // 100 ms — louder, brighter impact (backstab emphasis)
    int n = 100 * 22050 / 1000;
    std::vector<int16_t> s(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        double env = 1.0 - static_cast<double>(i) / n;
        double env2 = env * env;
        double sig = white_noise() * 0.5
                   + std::sin(2.0 * M_PI * 300.0 * i / 22050.0) * 0.3
                   + std::sin(2.0 * M_PI * 900.0 * i / 22050.0) * 0.2;
        s[static_cast<std::size_t>(i)] = static_cast<int16_t>(sig * 10000.0 * env2);
    }
    play_wav(make_wav(s));
}

void sound_throw() {
    debug_log("[sound] throw");
    // 120 ms — quick whoosh, mid-frequency swoosh with noise
    int n = 120 * 22050 / 1000;
    std::vector<int16_t> s(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        double t   = static_cast<double>(i) / 22050.0;
        double f   = 600.0 + static_cast<double>(i) / n * 400.0; // rising 600→1000 Hz
        double env = 1.0 - static_cast<double>(i) / n;
        double sig = std::sin(2.0 * M_PI * f * t) * 0.4 + white_noise() * 0.6;
        s[static_cast<std::size_t>(i)] = static_cast<int16_t>(sig * 5000.0 * env);
    }
    play_wav(make_wav(s));
}
