#include "logging.hpp"

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#ifndef NDEBUG
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#ifdef __linux__
#include <sys/statfs.h>
#endif

namespace tracking {
namespace logging {

namespace {

constexpr std::size_t kQueueSlots = 8192;  // Pre-allocated at init (§7.1).
constexpr int kRotatedFilesKept = 3;

// True when `path` is on a tmpfs mount. Only meaningful on Linux (the Pi 5
// target); elsewhere the check is skipped and no warning fires.
bool is_tmpfs(const std::string& path) {
#ifdef __linux__
    constexpr long kTmpfsMagic = 0x01021994;  // TMPFS_MAGIC (linux/magic.h)
    struct statfs info = {};
    if (statfs(path.c_str(), &info) != 0) {
        return false;  // Unstatable — treat as suspect so the warning fires.
    }
    return info.f_type == kTmpfsMagic;
#else
    (void)path;
    return true;
#endif
}

}  // namespace

void init(const LoggingConfig& config) {
    // Idempotent-by-replacement: drop any previous logger and thread pool so a
    // second init (test fixtures) cannot hit spdlog's duplicate-registration
    // throw or orphan an async logger on a dead pool.
    spdlog::shutdown();

    std::filesystem::create_directories(config.output_dir);
    const bool on_tmpfs = is_tmpfs(config.output_dir);

    spdlog::init_thread_pool(kQueueSlots, 1);

    const auto log_path =
        std::filesystem::path(config.output_dir) / "tracking_core.log";
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        log_path.string(),
        static_cast<std::size_t>(config.max_file_size_mb) * 1024 * 1024,
        kRotatedFilesKept));
#ifndef NDEBUG
    // Console output is a development affordance only — never in Release.
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
#endif

    // overrun_oldest: a full queue drops the eldest message instead of
    // blocking the producer — same staleness philosophy as ADR-002's
    // ZMQ_CONFLATE=1. Never block the hot path (§7.1).
    auto logger = std::make_shared<spdlog::async_logger>(
        "tracking_core", sinks.begin(), sinks.end(), spdlog::thread_pool(),
        spdlog::async_overflow_policy::overrun_oldest);

    const spdlog::level::level_enum level = spdlog::level::from_str(config.level);
    logger->set_level(level);
    logger->flush_on(spdlog::level::err);
    spdlog::set_default_logger(logger);
    spdlog::flush_every(std::chrono::seconds(3));

    if (!on_tmpfs) {
        // Persistent-storage logging violates §7.1 (never write the SD card on
        // the hot path). /tmp is tmpfs only from Debian trixie onward.
        SPDLOG_LOGGER_WARN(logger.get(),
                           "log output_dir '{}' is not tmpfs — logs are hitting "
                           "persistent storage, which violates the hot-path rule",
                           config.output_dir);
    }
    if (level < compiled_floor()) {
        SPDLOG_LOGGER_WARN(logger.get(),
                           "configured logging.level '{}' is below the compiled "
                           "floor '{}' — messages below the floor were compiled "
                           "out of this build",
                           config.level,
                           spdlog::level::to_string_view(compiled_floor()));
    }
}

void shutdown() { spdlog::shutdown(); }

spdlog::level::level_enum compiled_floor() {
    return static_cast<spdlog::level::level_enum>(SPDLOG_ACTIVE_LEVEL);
}

}  // namespace logging
}  // namespace tracking
