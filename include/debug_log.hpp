#pragma once

#include <string>

// Append a timestamped line to /tmp/bx_crawl_debug.log.
// Thread-safe (uses file-per-call, no shared state). No-op when disabled.
void debug_log(const std::string& msg);
void debug_log_set_enabled(bool e);
bool debug_log_enabled();
