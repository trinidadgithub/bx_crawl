#include "debug_log.hpp"

#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>

static bool g_debug_enabled = false;
static std::mutex g_mutex;

void debug_log_set_enabled(bool e) { g_debug_enabled = e; }
bool debug_log_enabled() { return g_debug_enabled; }

void debug_log(const std::string& msg) {
    if (!g_debug_enabled) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ofstream log("/tmp/bx_crawl_debug.log", std::ios::app);
    if (!log) return;
    auto now   = std::chrono::system_clock::now();
    auto tt    = std::chrono::system_clock::to_time_t(now);
    auto ms    = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now.time_since_epoch()) % 1000;
    auto tm    = *std::localtime(&tt);
    log << std::put_time(&tm, "%H:%M:%S") << '.'
        << std::setfill('0') << std::setw(3) << ms.count() << " "
        << msg << std::endl;
}
