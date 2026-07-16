// Proves the NDEBUG-derived default in logging.hpp (TRK-004 KTD-1). This TU
// deliberately sets no SPDLOG_ACTIVE_LEVEL override, so the header's own
// default branch is what compiles here — a wrong mapping (e.g. Release left
// at TRACE) fails this file's static_asserts in that build config, which the
// hand-set override in test_logging.cpp can never detect.

#include "logging.hpp"

#include <gtest/gtest.h>

#ifdef NDEBUG
static_assert(SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_INFO,
              "Release builds must compile out exactly DEBUG/TRACE (cpp rule §7.1)");
#else
static_assert(SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_TRACE,
              "Debug builds must keep all levels compiled in");
#endif

namespace {

TEST(LoggingDefaultLevel, CompileTimeFloorMatchesBuildType) {
    // The static_asserts above are the real test; this records the mapping in
    // the ctest run and checks the runtime accessor agrees with this TU.
    EXPECT_EQ(tracking::logging::compiled_floor(),
              static_cast<spdlog::level::level_enum>(SPDLOG_ACTIVE_LEVEL));
}

}  // namespace
