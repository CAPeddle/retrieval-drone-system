// TRK-031: replay integration — real ov5647 recordings through the TRK-008
// quality gate. Gated on TRACKING_REPLAY_DIR (set by tools/pi5-remote-test.sh
// on the Pi 5, where the recordings live); skips cleanly elsewhere so dev
// machines stay hermetic.
#include "config.hpp"
#include "frame_quality.hpp"
#include "replay_source.hpp"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>

#include <cstdlib>
#include <string>

namespace {

std::string replay_dir() {
    const char* dir = std::getenv("TRACKING_REPLAY_DIR");
    return dir != nullptr ? std::string(dir) : std::string();
}

// Plays a whole clip through the assessor and returns the dominant verdict
// counts. Buffers are sized from the first frame.
struct VerdictCounts {
    int good = 0;
    int degraded = 0;
    int reject = 0;
    int total = 0;
};

VerdictCounts assess_clip(const std::string& path) {
    VerdictCounts counts;
    tracking::ReplaySource source(path);
    if (!source.is_open()) {
        return counts;
    }
    cv::Mat frame;
    if (!source.grab(frame)) {
        return counts;
    }
    // The contract under test is the SHIPPED configuration against real
    // footage — not thresholds invented here. The template's blur default is
    // calibrated from measured ov5647 data (see tracking_core.yaml).
#ifdef TRACKING_CORE_CONFIG_TEMPLATE
    const tracking::FrameQualityConfig config =
        tracking::Config::load(TRACKING_CORE_CONFIG_TEMPLATE).frame_quality;
#else
    const tracking::FrameQualityConfig config{20.0, 240.0, 12.0};
#endif
    tracking::FrameQualityAssessor assessor(config, frame.rows, frame.cols);
    do {
        switch (assessor.assess(frame)) {
            case tracking::FrameQuality::GOOD: ++counts.good; break;
            case tracking::FrameQuality::DEGRADED: ++counts.degraded; break;
            case tracking::FrameQuality::REJECT: ++counts.reject; break;
        }
        ++counts.total;
    } while (source.grab(frame));
    return counts;
}

class ReplayIntegration : public ::testing::Test {
protected:
    void SetUp() override {
        if (replay_dir().empty()) {
            GTEST_SKIP() << "TRACKING_REPLAY_DIR unset — recordings live on the Pi 5";
        }
    }
};

TEST_F(ReplayIntegration, UnderexposedClipRejects) {
    const VerdictCounts counts = assess_clip(replay_dir() + "/underexposed.avi");
    ASSERT_GT(counts.total, 0) << "underexposed.avi missing or unreadable";
    // A deliberately starved exposure must overwhelmingly reject.
    EXPECT_GT(counts.reject, counts.total * 8 / 10)
        << "good=" << counts.good << " degraded=" << counts.degraded
        << " reject=" << counts.reject << " of " << counts.total;
}

TEST_F(ReplayIntegration, OverexposedClipRejects) {
    const VerdictCounts counts = assess_clip(replay_dir() + "/overexposed.avi");
    ASSERT_GT(counts.total, 0) << "overexposed.avi missing or unreadable";
    EXPECT_GT(counts.reject, counts.total * 8 / 10)
        << "good=" << counts.good << " degraded=" << counts.degraded
        << " reject=" << counts.reject << " of " << counts.total;
}

TEST_F(ReplayIntegration, NormalClipPassesTheGate) {
    const VerdictCounts counts = assess_clip(replay_dir() + "/normal.avi");
    ASSERT_GT(counts.total, 0) << "normal.avi missing or unreadable";
    // A normally-exposed static room scene should not be rejected for
    // exposure; blur depends on scene texture, so DEGRADED is acceptable.
    EXPECT_LT(counts.reject, counts.total / 5)
        << "good=" << counts.good << " degraded=" << counts.degraded
        << " reject=" << counts.reject << " of " << counts.total;
}

}  // namespace
