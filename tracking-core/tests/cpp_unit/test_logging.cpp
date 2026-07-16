// Unit tests for the TRK-004 async logger (src/core/logging.hpp/.cpp).
//
// This TU pins SPDLOG_ACTIVE_LEVEL to WARN *before* including logging.hpp —
// the header's #ifndef guard makes that the supported override — so the
// compile-time gate can be tested identically in Debug and Release builds.
// The header's own NDEBUG-derived default is proven by
// test_logging_default_level.cpp, which sets no override.
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_WARN

#include "logging.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

namespace fs = std::filesystem;

class LoggingTest : public ::testing::Test {
protected:
    tracking::LoggingConfig make_config(const std::string& level = "warn",
                                        int max_file_size_mb = 10) {
        tracking::LoggingConfig config;
        config.level = level;
        config.output_dir = ::testing::TempDir() + "trk004_" +
            ::testing::UnitTest::GetInstance()->current_test_info()->name();
        config.max_file_size_mb = max_file_size_mb;
        // The rotating sink appends: a log left by a previous run (or by the
        // other build config's test binary, which shares this path) would
        // leak stale lines into read_logs and corrupt content assertions.
        fs::remove_all(config.output_dir);
        temp_dirs_.push_back(config.output_dir);
        return config;
    }

    // Reads the concatenated contents of every log file under `dir` ("" if none).
    static std::string read_logs(const std::string& dir) {
        std::string all;
        if (!fs::exists(dir)) {
            return all;
        }
        for (const auto& entry : fs::directory_iterator(dir)) {
            std::ostringstream out;
            out << std::ifstream(entry.path()).rdbuf();
            all += out.str();
        }
        return all;
    }

    void TearDown() override {
        // KTD-6 re-init contract: each case inits its own logger; shutdown
        // drains and drops it so the next case starts clean.
        tracking::logging::shutdown();
        for (const std::string& dir : temp_dirs_) {
            fs::remove_all(dir);
        }
    }

private:
    std::vector<std::string> temp_dirs_;
};

TEST_F(LoggingTest, InitCreatesDirectoryAndWritesFile) {
    const tracking::LoggingConfig config = make_config();
    tracking::logging::init(config);
    EXPECT_TRUE(fs::exists(config.output_dir));

    LOG_WARN("hello from {}", "InitCreatesDirectoryAndWritesFile");
    tracking::logging::shutdown();  // drains the async queue and flushes

    const std::string logs = read_logs(config.output_dir);
    EXPECT_NE(logs.find("hello from InitCreatesDirectoryAndWritesFile"),
              std::string::npos);
}

TEST_F(LoggingTest, RepeatedInitReplacesLogger) {
    // Two inits in one process must not throw (spdlog duplicate-registration
    // is the classic failure); the second config wins.
    tracking::logging::init(make_config());
    LOG_WARN("first logger");
    const tracking::LoggingConfig second = make_config();
    ASSERT_NO_THROW(tracking::logging::init(second));
    LOG_WARN("second logger");
    tracking::logging::shutdown();

    EXPECT_NE(read_logs(second.output_dir).find("second logger"), std::string::npos);
}

TEST_F(LoggingTest, EnqueueDoesNotBlockCallingThread) {
    tracking::logging::init(make_config());

    // 1,000 enqueues must complete far faster than 1,000 synchronous file
    // writes. The 250 ms bound is deliberately generous (loaded dev machine,
    // Pi 5) — it distinguishes "enqueue" from "write", nothing finer. A
    // failure here is signal, not noise: do not widen the bound (plan stop
    // condition), report the measurement instead.
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        LOG_WARN("timing message {}", i);
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
              250);
}

TEST_F(LoggingTest, CompileTimeGateSkipsArgumentEvaluation) {
    const tracking::LoggingConfig config = make_config("trace");
    tracking::logging::init(config);

    int evaluations = 0;
    // With this TU's active level at WARN, LOG_DEBUG compiles to nothing —
    // the argument expression must never run, even with the runtime level at
    // trace.
    LOG_DEBUG("gated message {}", ++evaluations);
    EXPECT_EQ(evaluations, 0);

    tracking::logging::shutdown();
    EXPECT_EQ(read_logs(config.output_dir).find("gated message"), std::string::npos);
}

TEST_F(LoggingTest, RuntimeLevelFilters) {
    const tracking::LoggingConfig config = make_config("error");
    tracking::logging::init(config);

    LOG_WARN("quiet-warn should not appear");
    LOG_ERROR("boom-error should appear");
    tracking::logging::shutdown();

    const std::string logs = read_logs(config.output_dir);
    EXPECT_EQ(logs.find("quiet-warn"), std::string::npos);
    EXPECT_NE(logs.find("boom-error"), std::string::npos);
}

#ifdef __linux__
TEST_F(LoggingTest, IsTmpfsDetectsRealMounts) {
    // /dev/shm is tmpfs on every Linux target we run on (incl. Pi OS).
    EXPECT_TRUE(tracking::logging::is_tmpfs("/dev/shm"));
    // A path that does not exist is unstatable and must read as suspect.
    EXPECT_FALSE(tracking::logging::is_tmpfs("/nonexistent/trk004"));
}
#endif

TEST_F(LoggingTest, InitWarningsSurviveStrictRuntimeLevel) {
    // Regression for the set_level ordering bug: with logging.level=error the
    // init-time safety warnings must still reach the sink. The tmpfs warning
    // fires exactly when the environment's temp dir is not tmpfs, so assert
    // agreement with is_tmpfs() rather than a fixed expectation.
    const tracking::LoggingConfig config = make_config("error");
    tracking::logging::init(config);
    LOG_WARN("post-init warn must be filtered");
    tracking::logging::shutdown();

    const std::string logs = read_logs(config.output_dir);
    const bool expect_tmpfs_warning = !tracking::logging::is_tmpfs(config.output_dir);
    EXPECT_EQ(logs.find("not tmpfs") != std::string::npos, expect_tmpfs_warning);
    // The configured level still applies to everything after init.
    EXPECT_EQ(logs.find("post-init warn"), std::string::npos);
}

TEST_F(LoggingTest, LogAfterShutdownFallsBackWithoutCrash) {
    tracking::logging::init(make_config());
    tracking::logging::shutdown();
    // Must not crash: shutdown installs a synchronous stderr fallback.
    LOG_ERROR("late message after shutdown (expected on stderr)");
    ASSERT_NE(tracking::logging::get(), nullptr);
}

TEST_F(LoggingTest, WarnsWhenConfiguredLevelBelowCompiledFloor) {
    // 'trace' is below the compiled floor exactly when logging.cpp was built
    // with a floor above trace (Release: info). Branch on the actual floor so
    // this test is meaningful in both build configs.
    const bool expect_warning =
        spdlog::level::trace < tracking::logging::compiled_floor();

    const tracking::LoggingConfig config = make_config("trace");
    tracking::logging::init(config);
    tracking::logging::shutdown();

    const bool warned =
        read_logs(config.output_dir).find("compiled floor") != std::string::npos;
    EXPECT_EQ(warned, expect_warning);
}

TEST_F(LoggingTest, OverflowFloodNeverBlocks) {
    tracking::logging::init(make_config());

    // Flood well past the 8192-slot queue. overrun_oldest drops the eldest
    // instead of blocking, so this must finish quickly; message loss is
    // expected and not asserted.
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 20000; ++i) {
        LOG_WARN("flood {}", i);
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
              2000);
}

TEST_F(LoggingTest, RotationProducesRotatedFile) {
    const tracking::LoggingConfig config = make_config("warn", /*max_file_size_mb=*/1);
    tracking::logging::init(config);

    // ~3 MB in paced batches (each batch far below the queue capacity, with a
    // pause for the worker to drain) so rotation happens from real writes
    // rather than being defeated by overrun_oldest drops.
    const std::string payload(200, 'x');
    for (int batch = 0; batch < 40; ++batch) {
        for (int i = 0; i < 400; ++i) {
            LOG_WARN("{} {}", payload, batch * 400 + i);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    tracking::logging::shutdown();

    bool rotated = false;
    for (const auto& entry : fs::directory_iterator(config.output_dir)) {
        if (entry.path().filename().string().find(".1.") != std::string::npos) {
            rotated = true;
        }
    }
    EXPECT_TRUE(rotated) << "expected a rotated tracking_core.1.log alongside the active file";
}

}  // namespace
