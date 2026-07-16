#pragma once

// TRK-004 async logger — project-wide log macros over spdlog (cpp rule §7.1:
// async, ring-buffered; DEBUG/TRACE compiled out of Release builds). "Never
// blocks" means never waits for queue space (overrun_oldest drops the eldest);
// the enqueue itself takes a short std::mutex critical section shared with the
// worker — not strictly lock-free. SCHED_FIFO threads logging through it risk
// priority inversion; revisit before TRK-006's capture thread adopts LOG_*.
//
// Compile-time floor. Release (NDEBUG) compiles out exactly DEBUG/TRACE; INFO
// and above stay runtime-filtered by logging.level, so the config surface
// remains meaningful on the deployed binary. A TU may pre-define
// SPDLOG_ACTIVE_LEVEL before including this header to override (the gating
// unit test does). Always include this header instead of a raw spdlog header —
// a TU that includes spdlog first silently loses the floor override.
#ifndef SPDLOG_ACTIVE_LEVEL
#ifdef NDEBUG
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#endif

#include "config.hpp"

#include <spdlog/spdlog.h>

namespace tracking {
namespace logging {

// Creates the async logger from config and registers it as spdlog's default:
// pre-allocated 8192-slot queue on one worker thread, rotating file sink under
// config.output_dir (created if absent; warns once when it is not tmpfs), a
// stdout sink in Debug builds only, overrun_oldest overflow (never block the
// caller — same staleness philosophy as ADR-002's ZMQ_CONFLATE=1), flush on
// error plus a 3 s periodic flush.
//
// Startup-only: may throw std::filesystem::filesystem_error or
// spdlog::spdlog_ex. Call once from main() before any LOG_* use; not
// thread-safe against concurrent first use. Calling again replaces the
// previous logger (test fixtures rely on this); production calls it once.
void init(const LoggingConfig& config);

// Drains the async queue, flushes sinks, and releases the logger, then
// installs a synchronous stderr fallback as the default — a stray LOG_* after
// shutdown degrades to stderr instead of crashing on spdlog's nulled default.
void shutdown();

// The logger the LOG_* macros target. Non-owning; the async logger between
// init and shutdown, spdlog's built-in stdout default before the first init,
// and the synchronous stderr fallback after shutdown.
inline spdlog::logger* get() { return spdlog::default_logger_raw(); }

// True when `path` sits on a tmpfs mount (Linux; other platforms report true).
// Exposed for tests; init uses it for the §7.1 persistent-storage warning.
bool is_tmpfs(const std::string& path);

// The floor this library was compiled with — messages below it are compiled
// out of logging.cpp's TU and the LOG_* macros of every TU using the default.
spdlog::level::level_enum compiled_floor();

}  // namespace logging
}  // namespace tracking

// Hot-path log macros (§7.1). Below SPDLOG_ACTIVE_LEVEL these expand to
// nothing — the format arguments are never evaluated.
#define LOG_TRACE(...)    SPDLOG_LOGGER_TRACE(::tracking::logging::get(), __VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_LOGGER_DEBUG(::tracking::logging::get(), __VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_LOGGER_INFO(::tracking::logging::get(), __VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_LOGGER_WARN(::tracking::logging::get(), __VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_LOGGER_ERROR(::tracking::logging::get(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(::tracking::logging::get(), __VA_ARGS__)
