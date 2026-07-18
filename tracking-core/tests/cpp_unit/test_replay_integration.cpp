// TRK-031: replay integration — real ov5647 recordings through the TRK-008
// quality gate. Gated on TRACKING_REPLAY_DIR (set by tools/pi5-remote-test.sh
// on the Pi 5, where the recordings live); skips cleanly elsewhere so dev
// machines stay hermetic.
#include "ball_detector.hpp"
#include "calibration_marker_detector.hpp"
#include "config.hpp"
#include "frame_quality.hpp"
#include "replay_source.hpp"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

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

// Loads the shipped template config; the gates test the shipped defaults
// against real footage, not thresholds invented here (KTD-6). Falls back to a
// hand-built config off-template so the file still compiles everywhere.
tracking::Config shipped_config() {
#ifdef TRACKING_CORE_CONFIG_TEMPLATE
    return tracking::Config::load(TRACKING_CORE_CONFIG_TEMPLATE);
#else
    tracking::Config c;
    c.frame_quality = {20.0, 240.0, 12.0};
    c.ball = {0.03, 10, 80, 0.82, 5, 200};
    c.calibration.aruco_dictionary = "4X4_50";
    c.calibration.marker_ids = {0};
    return c;
#endif
}

class ReplayIntegration : public ::testing::Test {
protected:
    void SetUp() override {
        if (replay_dir().empty()) {
            GTEST_SKIP() << "TRACKING_REPLAY_DIR unset — recordings live on the Pi 5";
        }
    }
};

// Detection false-positive counts over a clip, restricted to the frames the
// production pipeline would actually feed the detectors (GOOD/DEGRADED — the
// quality gate rejects the rest first). Matches the runtime contract.
struct DetectionCounts {
    int frames_assessed = 0;   // frames that passed the quality gate
    int ball_detections = 0;
    int marker_detections = 0;
};

DetectionCounts detect_over_clip(const std::string& path, const tracking::Config& config) {
    DetectionCounts counts;
    tracking::ReplaySource source(path);
    cv::Mat frame;
    if (!source.is_open() || !source.grab(frame)) {
        return counts;
    }
    tracking::FrameQualityAssessor quality(config.frame_quality, frame.rows, frame.cols);
    tracking::BallDetector ball(config.ball, frame.rows, frame.cols);
    tracking::CalibrationMarkerDetector markers(config.calibration);
    do {
        if (quality.assess(frame) == tracking::FrameQuality::REJECT) {
            continue;  // production never feeds rejected frames to detection
        }
        ++counts.frames_assessed;
        if (ball.detect(frame).has_value()) {
            ++counts.ball_detections;
        }
        counts.marker_detections += static_cast<int>(markers.detect(frame).size());
    } while (source.grab(frame));
    return counts;
}

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

// --- TRK-010/011 replay false-positive gates (U3) --------------------------
// The shipped detector defaults must not fire on a ball-free, marker-free room
// scene. A failure here is calibration signal, handled by the plan's
// gate-failure protocol (record counts, done-with-deferral) — never tune the
// thresholds to pass.

TEST_F(ReplayIntegration, NormalClipNoBallFalsePositives) {
    const DetectionCounts counts =
        detect_over_clip(replay_dir() + "/normal.avi", shipped_config());
    ASSERT_GT(counts.frames_assessed, 0) << "normal.avi missing, unreadable, or all-rejected";
    EXPECT_EQ(counts.ball_detections, 0)
        << counts.ball_detections << " ball false positives in "
        << counts.frames_assessed << " quality-passed frames";
}

TEST_F(ReplayIntegration, NormalClipNoMarkerFalsePositives) {
    const DetectionCounts counts =
        detect_over_clip(replay_dir() + "/normal.avi", shipped_config());
    ASSERT_GT(counts.frames_assessed, 0) << "normal.avi missing, unreadable, or all-rejected";
    EXPECT_EQ(counts.marker_detections, 0)
        << counts.marker_detections << " marker false positives in "
        << counts.frames_assessed << " quality-passed frames";
}

// Informational only: raw underexposed frames (which production rejects at the
// quality gate) are not asserted — logged so a phantom-blob regression is
// visible without failing the build on out-of-contract input.
TEST_F(ReplayIntegration, UnderexposedRawDetectionCountsAreInformational) {
    tracking::ReplaySource source(replay_dir() + "/underexposed.avi");
    cv::Mat frame;
    ASSERT_TRUE(source.is_open() && source.grab(frame));
    const tracking::Config config = shipped_config();
    tracking::BallDetector ball(config.ball, frame.rows, frame.cols);
    int raw_balls = 0, frames = 0;
    do {
        ++frames;
        if (ball.detect(frame).has_value()) ++raw_balls;
    } while (source.grab(frame));
    RecordProperty("underexposed_raw_ball_detections", raw_balls);
    RecordProperty("underexposed_frames", frames);
    SUCCEED() << "raw underexposed ball detections (informational): " << raw_balls
              << " of " << frames;
}

// True-positive counterweight: a uselessly strict default set (narrow radius
// band, very high circularity) would sail through the FP gates while shipping a
// detector with no recall. Paste the synthetic ball onto real GOOD frames and
// require it be found.
TEST_F(ReplayIntegration, CompositeBallIsDetectedOnRealFrames) {
    const tracking::Config config = shipped_config();
    tracking::ReplaySource source(replay_dir() + "/normal.avi");
    cv::Mat frame;
    ASSERT_TRUE(source.is_open() && source.grab(frame));
    tracking::FrameQualityAssessor quality(config.frame_quality, frame.rows, frame.cols);
    tracking::BallDetector ball(config.ball, frame.rows, frame.cols);

    const int paste_radius =
        (config.ball.expected_radius_px_min + config.ball.expected_radius_px_max) / 2;
    const cv::Point paste_at(frame.cols / 2, frame.rows / 2);
    int composites = 0, detected = 0;
    do {
        if (quality.assess(frame) == tracking::FrameQuality::REJECT) {
            continue;
        }
        cv::Mat composite = frame.clone();
        // A real ball occludes whatever is behind it, including any bright room
        // clutter — model that by clearing a small halo before drawing the disc
        // so the synthetic ball is isolated (without it, the disc merges with
        // bright background regions and its circularity is destroyed, which
        // tests scene interaction rather than detector recall).
        cv::circle(composite, paste_at, paste_radius + 8, cv::Scalar::all(0), cv::FILLED);
        cv::circle(composite, paste_at, paste_radius, cv::Scalar::all(255), cv::FILLED);
        ++composites;
        const auto obs = ball.detect(composite);
        if (obs.has_value() && std::hypot(obs->centroid_px.x - paste_at.x,
                                          obs->centroid_px.y - paste_at.y) < 2.0) {
            ++detected;
        }
        if (composites >= 10) {
            break;
        }
    } while (source.grab(frame));

    ASSERT_GE(composites, 10) << "fewer than 10 GOOD frames to composite onto";
    EXPECT_GE(detected, 9)  // >= 90%
        << "shipped ball defaults detected the pasted ball in only " << detected
        << " of " << composites << " composites — defaults may be too strict";
}

}  // namespace
