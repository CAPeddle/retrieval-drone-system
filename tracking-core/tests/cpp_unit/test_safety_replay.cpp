// TRK-020 / plan U12: the safety Replay Gate and the True-Positive
// Counterweight, both through the full production pipeline (quality ->
// detection -> observation build -> tracking -> mapping -> snapshot ->
// predicate). Determinism: a synthetic frame-derived clock (KTD-6) — now_ns
// and injected-observation timestamps come from the frame sequence, never
// steady_clock, so outcomes depend only on clip content, not test-host load.
//
// The gate over the recorded Pi clips is vacuous for the laser clauses by
// design (no laser detector in v0.3): it proves the pipeline never fabricates
// safe_for_control=true from real footage. The injected suites provide the
// counterweight — "always false" cannot pass. House rule: a gate failure is
// calibration signal; record the counts, never tune thresholds.

#include <gtest/gtest.h>

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "ball_detector.hpp"
#include "calibration_fixture.hpp"
#include "config.hpp"
#include "coordinate_mapper.hpp"
#include "frame_quality.hpp"
#include "homography_loader.hpp"
#include "safe_for_control.hpp"
#include "snapshot_builder.hpp"
#include "tracker.hpp"

namespace {

namespace fx = calibration_fixture;

constexpr std::int64_t kMs = 1'000'000;

// Loads the shipped config template (KTD-6 of the detection plan: test the
// shipped defaults, never thresholds invented here).
tracking::Config shipped_config() {
    return tracking::Config::load(TRACKING_CORE_CONFIG_TEMPLATE);
}

struct HarnessResult {
    std::uint64_t frames = 0;
    std::uint64_t admitted = 0;
    std::uint64_t safe_true_frames = 0;
    std::uint64_t reason_counts[8] = {};
    std::int64_t first_safe_ns = -1;
    tracking::SafetyResult last_safety;
};

// Synchronous full-pipeline harness (plan U12): frames in order, synthetic
// clock, fixture calibration artifacts loaded through the real loader.
class SafetyHarness {
public:
    SafetyHarness(const tracking::Config& config, std::int64_t frame_step_ns)
        : config_(config),
          frame_step_ns_(frame_step_ns),
          quality_(config.frame_quality, config.camera.height, config.camera.width),
          detector_(config.ball, config.camera.height, config.camera.width),
          tracker_(config.track, config.gating),
          artifacts_(load_fixture_artifacts()),
          mapper_(artifacts_, fx::coordinate_config(), config.ball.radius_m),
          builder_(mapper_, fx::coordinate_config()),
          evaluator_(config.safe_for_control, config.ball.radius_m) {
        observations_.reserve(tracking::Tracker::kMaxObservations);
    }

    // One frame through the production path; `injected` enters at the same
    // observation-build seam main.cpp uses, stamped with the synthetic clock.
    void process(const cv::Mat& frame,
                 const std::vector<tracking::Observation>& injected = {}) {
        const std::int64_t t = now_ns_;
        now_ns_ += frame_step_ns_;
        ++result_.frames;

        if (quality_.assess(frame) == tracking::FrameQuality::REJECT) {
            builder_.note_frame_rejected();
            observations_.clear();
            tracker_.update(observations_, t);
            return;
        }
        builder_.mark_frame_admitted();
        builder_.note_frame_processed();
        ++result_.admitted;

        observations_.clear();
        const std::optional<tracking::BallObservation> ball = detector_.detect(frame);
        if (ball.has_value()) {
            tracking::Observation obs;
            obs.type = tracking::ObjectType::Ball;
            obs.centroid_px = ball->centroid_px;
            obs.radius_px = ball->radius_px;
            obs.capture_timestamp_ns = t;
            observations_.push_back(obs);
        }
        for (tracking::Observation obs : injected) {
            obs.capture_timestamp_ns = t;  // synthetic clock, same as production path
            observations_.push_back(obs);
        }

        const auto& tracks = tracker_.update(observations_, t);
        tracking::TrackingSnapshot snapshot = builder_.build(tracks, t, t);
        const tracking::SafetyResult safety = evaluator_.evaluate(snapshot, t);
        result_.last_safety = safety;
        if (safety.safe) {
            ++result_.safe_true_frames;
            if (result_.first_safe_ns < 0) {
                result_.first_safe_ns = t;
            }
        }
        for (int bit = 0; bit < 8; ++bit) {
            if (safety.unsafe_reasons & (1U << bit)) {
                ++result_.reason_counts[bit];
            }
        }
    }

    const HarnessResult& result() const { return result_; }
    std::int64_t now_ns() const { return now_ns_; }

private:
    static tracking::CalibrationArtifacts load_fixture_artifacts() {
        const std::string dir = ::testing::TempDir();
        const std::string ipath =
            fx::write_json(dir, "trk020_fixture_intrinsics.json", fx::intrinsics_json());
        const std::string epath =
            fx::write_json(dir, "trk020_fixture_extrinsics.json", fx::extrinsics_json());
        tracking::CalibrationArtifacts artifacts = tracking::load_calibration_artifacts(
            ipath, epath, fx::coordinate_config());
        std::remove(ipath.c_str());
        std::remove(epath.c_str());
        return artifacts;
    }

    tracking::Config config_;
    std::int64_t frame_step_ns_;
    std::int64_t now_ns_ = 0;
    tracking::FrameQualityAssessor quality_;
    tracking::BallDetector detector_;
    tracking::Tracker tracker_;
    tracking::CalibrationArtifacts artifacts_;
    tracking::CoordinateMapper mapper_;
    tracking::SnapshotBuilder builder_;
    tracking::SafeForControlEvaluator evaluator_;
    std::vector<tracking::Observation> observations_;
    HarnessResult result_;
};

// ── Synthetic scenario helpers (deterministic, seeded) ───────────────────────

cv::Mat synthetic_frame(cv::RNG& rng, const tracking::Config& config, bool with_ball) {
    cv::Mat frame(config.camera.height, config.camera.width, CV_8UC3);
    rng.fill(frame, cv::RNG::NORMAL, 100, 25);
    if (with_ball) {
        const cv::Point2d px = fx::pixel_of_world({0.0, 2.0, 0.03});
        cv::circle(frame, cv::Point(static_cast<int>(px.x), static_cast<int>(px.y)), 20,
                   cv::Scalar(255, 255, 255), cv::FILLED, cv::LINE_AA);
    }
    return frame;
}

// A laser observation whose Z=0 floor projection lands at (x_m, y_m).
tracking::Observation laser_at_floor(double x_m, double y_m) {
    const cv::Point2d px = fx::pixel_of_world({x_m, y_m, 0.0});
    tracking::Observation obs;
    obs.type = tracking::ObjectType::Laser;
    obs.centroid_px = {static_cast<float>(px.x), static_cast<float>(px.y)};
    obs.radius_px = 3.0;
    return obs;
}

// ── True-Positive Counterweight (success criterion 4) ────────────────────────

TEST(SafetyCounterweight, AlignedStationaryLaserAndBallFlipTrueAfterDwell) {
    // "Detect nothing" cannot satisfy the gates: every clause passes through
    // the production path and the predicate flips true exactly once the dwell
    // is served. Laser floor point 1 cm from the ball centre (< 5 cm envelope).
    tracking::Config config = shipped_config();
    SafetyHarness harness(config, 16'700'000);  // ~60 fps synthetic cadence
    cv::RNG rng(12345);

    for (int i = 0; i < 30; ++i) {  // ~500 ms
        harness.process(synthetic_frame(rng, config, true),
                        {laser_at_floor(0.01, 2.0)});
    }
    const HarnessResult& result = harness.result();
    EXPECT_GT(result.safe_true_frames, 0U)
        << "predicate never flipped true through the production path";
    // The dwell (200 ms from RUNNING entry) plus confirmation must be served
    // first: no safe frame before 200 ms on the synthetic clock.
    ASSERT_GE(result.first_safe_ns, 200 * kMs);
    EXPECT_TRUE(result.last_safety.safe);
    EXPECT_NEAR(result.last_safety.laser_ball_distance_m, 0.01, 0.01);
}

TEST(SafetyCounterweight, MisalignmentFlipsFalseImmediately) {
    tracking::Config config = shipped_config();
    SafetyHarness harness(config, 16'700'000);
    cv::RNG rng(12345);
    for (int i = 0; i < 30; ++i) {
        harness.process(synthetic_frame(rng, config, true), {laser_at_floor(0.01, 2.0)});
    }
    ASSERT_TRUE(harness.result().last_safety.safe);

    // Laser moves 7 cm: still inside the ~8 cm association gate (so the track
    // stays Confirmed) but outside the 5 cm alignment envelope — misaligned on
    // the very next evaluation. (A larger jump breaks association instead and
    // goes unsafe via NotConfirmed — equally conservative, different clause.)
    harness.process(synthetic_frame(rng, config, true), {laser_at_floor(0.07, 2.0)});
    EXPECT_FALSE(harness.result().last_safety.safe);
    EXPECT_TRUE(harness.result().last_safety.unsafe_reasons &
                tracking::kLaserBallMisaligned);
}

TEST(SafetyCounterweight, StaleLaserGoesUnsafeByAge) {
    tracking::Config config = shipped_config();
    SafetyHarness harness(config, 16'700'000);
    cv::RNG rng(12345);
    for (int i = 0; i < 30; ++i) {
        harness.process(synthetic_frame(rng, config, true), {laser_at_floor(0.01, 2.0)});
    }
    ASSERT_TRUE(harness.result().last_safety.safe);

    // Injection stops: the laser track ages; the predicate must go false
    // within age_max (50 ms ~= 3 synthetic frames) and stay false.
    for (int i = 0; i < 10; ++i) {
        harness.process(synthetic_frame(rng, config, true));
    }
    const HarnessResult& result = harness.result();
    EXPECT_FALSE(result.last_safety.safe);
    EXPECT_TRUE(result.last_safety.unsafe_reasons &
                (tracking::kAgeExceedsThreshold | tracking::kLaserNotConfirmed));
}

TEST(SafetyCounterweight, MovingLaserIsInTransit) {
    tracking::Config config = shipped_config();
    SafetyHarness harness(config, 16'700'000);
    cv::RNG rng(12345);
    // Laser sweeping 2 cm per frame (~1.2 m/s >> 0.05 m/s settled threshold).
    for (int i = 0; i < 30; ++i) {
        harness.process(synthetic_frame(rng, config, true),
                        {laser_at_floor(0.01 + 0.02 * i, 2.0)});
    }
    const HarnessResult& result = harness.result();
    EXPECT_EQ(result.safe_true_frames, 0U);
    EXPECT_GT(result.reason_counts[5], 0U);  // kLaserInTransit bit
}

TEST(PipelineThroughput, ProcessingSustainsTargetFrameRate) {
    // Plan U17 / success criterion 2 (processing side): the per-frame cost of
    // quality -> detect -> track -> map -> snapshot -> predicate must sustain
    // >= 60 fps. Capture arrives via its own thread in production, so this
    // measures the main-loop budget. Asserted in Release only; Debug records.
    tracking::Config config = shipped_config();
    SafetyHarness harness(config, 16'700'000);
    cv::RNG rng(20260718);
    // Pre-render a small frame pool so frame synthesis is outside the timing.
    std::vector<cv::Mat> frames;
    for (int i = 0; i < 8; ++i) {
        frames.push_back(synthetic_frame(rng, config, true));
    }
    constexpr int kFrames = 600;  // ~10 s of 60 fps work
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kFrames; ++i) {
        harness.process(frames[static_cast<std::size_t>(i) % frames.size()],
                        {laser_at_floor(0.01, 2.0)});
    }
    const double total_s =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const double fps = kFrames / total_s;
    ::testing::Test::RecordProperty("processing_fps", static_cast<int>(fps));
#ifdef NDEBUG
    EXPECT_GE(fps, 60.0) << "main-loop processing cannot sustain 60 fps";
#endif
}

// ── The Replay Gate (ADR-007; success criterion 3 — interim evidence) ────────

class SafetyReplayGate : public ::testing::TestWithParam<const char*> {
protected:
    void SetUp() override {
        const char* dir = std::getenv("TRACKING_REPLAY_DIR");
        if (dir == nullptr) {
            GTEST_SKIP() << "TRACKING_REPLAY_DIR unset — recordings live on the Pi 5";
        }
        clip_path_ = std::string(dir) + "/" + GetParam();
    }
    std::string clip_path_;
};

TEST_P(SafetyReplayGate, ZeroSafeForControlTruePositives) {
    cv::VideoCapture capture(clip_path_);
    if (!capture.isOpened()) {
        GTEST_SKIP() << "clip missing: " << clip_path_;
    }
    tracking::Config config = shipped_config();
    // Clip-native cadence (the Pi 3B recordings are 15 fps); the synthetic
    // clock steps at exactly that rate so ages reflect clip time, not test
    // wall time. cv::VideoCapture is used directly (not ReplaySource) so no
    // real-time pacing or real clocks enter the harness.
    double fps = capture.get(cv::CAP_PROP_FPS);
    if (!(fps > 0.0)) {
        fps = 15.0;
    }
    SafetyHarness harness(config, static_cast<std::int64_t>(1.0e9 / fps));

    cv::Mat frame;
    while (capture.read(frame)) {
        if (frame.rows != config.camera.height || frame.cols != config.camera.width) {
            GTEST_SKIP() << "clip geometry mismatch: " << frame.cols << "x" << frame.rows;
        }
        harness.process(frame);
    }
    const HarnessResult& result = harness.result();
    ASSERT_GT(result.frames, 0U);

    // THE gate (ADR-007): zero frames with safe_for_control == true. A failure
    // here is calibration signal — report the counts below, never tune.
    EXPECT_EQ(result.safe_true_frames, 0U)
        << "frames=" << result.frames << " admitted=" << result.admitted;

    // Informational context for gate interpretation (never asserted).
    RecordProperty("frames", static_cast<int>(result.frames));
    RecordProperty("admitted", static_cast<int>(result.admitted));
    RecordProperty("reason_laser_not_confirmed", static_cast<int>(result.reason_counts[2]));
    RecordProperty("reason_ball_not_confirmed", static_cast<int>(result.reason_counts[3]));
    RecordProperty("reason_age", static_cast<int>(result.reason_counts[4]));
}

INSTANTIATE_TEST_SUITE_P(RecordedScenarios, SafetyReplayGate,
                         ::testing::Values("normal.avi", "underexposed.avi",
                                           "overexposed.avi"));

}  // namespace
