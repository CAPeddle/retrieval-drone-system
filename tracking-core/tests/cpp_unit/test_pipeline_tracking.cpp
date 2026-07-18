// TRK-014..016 / plan U4: quality -> detection -> tracking pipeline semantics,
// driven frame-by-frame with synthetic imagery mirroring the main-loop policy
// (tick every iteration; REJECT frames still age tracks — plan R4). Frames are
// generated in-memory with seeded noise for determinism; clip-driven replay
// arrives with the Phase C harness. The composited ball is drawn opaque over
// the background, modelling occlusion per the composite-targets solution doc.

#include "ball_detector.hpp"
#include "config.hpp"
#include "frame_quality.hpp"
#include "tracker.hpp"

#include <gtest/gtest.h>

#include <opencv2/imgproc.hpp>

#include <cstdint>
#include <vector>

namespace {

constexpr std::int64_t kFrameNs = 16'700'000;  // ~60 fps cadence

class PipelineTrackingTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = tracking::Config::load(TRACKING_CORE_CONFIG_TEMPLATE);
        quality_.emplace(config_.frame_quality, config_.camera.height,
                         config_.camera.width);
        detector_.emplace(config_.ball, config_.camera.height, config_.camera.width);
        tracker_.emplace(config_.track, config_.gating);
        observations_.reserve(tracking::Tracker::kMaxObservations);
    }

    // Textured background (seeded noise) passes the shipped blur/exposure
    // gates; the optional ball is a bright opaque disc the detector accepts.
    cv::Mat make_frame(bool with_ball, float x = 0.0F, float y = 0.0F) {
        cv::Mat frame(config_.camera.height, config_.camera.width, CV_8UC3);
        rng_.fill(frame, cv::RNG::NORMAL, 100, 25);
        if (with_ball) {
            cv::circle(frame, cv::Point(static_cast<int>(x), static_cast<int>(y)), 20,
                       cv::Scalar(255, 255, 255), cv::FILLED, cv::LINE_AA);
        }
        return frame;
    }

    // Flat frame: zero Laplacian variance -> REJECT at the shipped blur gate.
    cv::Mat make_flat_frame() {
        return cv::Mat(config_.camera.height, config_.camera.width, CV_8UC3,
                       cv::Scalar(100, 100, 100));
    }

    // One main-loop iteration at capture time t_ns (mirrors main.cpp policy).
    void process(const cv::Mat& frame, std::int64_t t_ns) {
        observations_.clear();
        if (quality_->assess(frame) != tracking::FrameQuality::REJECT) {
            const auto ball = detector_->detect(frame);
            if (ball.has_value()) {
                tracking::Observation obs;
                obs.type = tracking::ObjectType::Ball;
                obs.centroid_px = ball->centroid_px;
                obs.radius_px = ball->radius_px;
                obs.capture_timestamp_ns = t_ns;
                observations_.push_back(obs);
            }
        }
        tracker_->update(observations_, t_ns);
    }

    const tracking::Track* ball_track() const {
        for (const tracking::Track& track : tracker_->tracks()) {
            if (track.type() == tracking::ObjectType::Ball) {
                return &track;
            }
        }
        return nullptr;
    }

    tracking::Config config_;
    std::optional<tracking::FrameQualityAssessor> quality_;
    std::optional<tracking::BallDetector> detector_;
    std::optional<tracking::Tracker> tracker_;
    std::vector<tracking::Observation> observations_;
    cv::RNG rng_{12345};  // seeded: deterministic frames
};

TEST_F(PipelineTrackingTest, MovingBallConfirmsWithinThreshold) {
    std::int64_t t = 0;
    for (int i = 0; i < config_.track.confirm_threshold; ++i, t += kFrameNs) {
        process(make_frame(true, 320.0F + 5.0F * i, 240.0F), t);
    }
    const tracking::Track* track = ball_track();
    ASSERT_NE(track, nullptr) << "detector never fired on the composited ball";
    EXPECT_EQ(track->state(), tracking::TrackState::Confirmed);
    EXPECT_NEAR(track->position_px().x, 320.0F + 5.0F * (config_.track.confirm_threshold - 1),
                4.0F);
}

TEST_F(PipelineTrackingTest, DecaysOnScheduleAfterBallDisappears) {
    std::int64_t t = 0;
    for (int i = 0; i < 5; ++i, t += kFrameNs) {
        process(make_frame(true, 320.0F, 240.0F), t);
    }
    ASSERT_NE(ball_track(), nullptr);
    ASSERT_EQ(ball_track()->state(), tracking::TrackState::Confirmed);
    const std::int64_t last_obs = ball_track()->last_observation_ns();

    // Ball vanishes; frames stay admitted (noise background). Decay follows
    // the capture-clock schedule: Predicted immediately, Occluded at >=50 ms,
    // Lost at >=200 ms (ties decay).
    process(make_frame(false), t);
    ASSERT_NE(ball_track(), nullptr);
    EXPECT_EQ(ball_track()->state(), tracking::TrackState::Predicted);
    t += kFrameNs;

    while (t - last_obs < 200 * 1'000'000) {
        process(make_frame(false), t);
        const tracking::Track* track = ball_track();
        ASSERT_NE(track, nullptr);
        if (t - last_obs >= 50 * 1'000'000) {
            EXPECT_EQ(track->state(), tracking::TrackState::Occluded)
                << "age_ms=" << track->age_since_last_observation_ms(t);
        }
        t += kFrameNs;
    }
    process(make_frame(false), t);
    ASSERT_NE(ball_track(), nullptr);
    EXPECT_EQ(ball_track()->state(), tracking::TrackState::Lost);
}

TEST_F(PipelineTrackingTest, RejectBurstAgesTrackInsteadOfFreezingIt) {
    std::int64_t t = 0;
    for (int i = 0; i < 5; ++i, t += kFrameNs) {
        process(make_frame(true, 320.0F, 240.0F), t);
    }
    ASSERT_EQ(ball_track()->state(), tracking::TrackState::Confirmed);
    const std::int64_t last_obs = ball_track()->last_observation_ns();

    // 300 ms of REJECT-quality frames: the track must age through the decay
    // chain (plan R4), not sit frozen at Confirmed.
    while (t - last_obs < 300 * 1'000'000) {
        process(make_flat_frame(), t);
        t += kFrameNs;
    }
    const tracking::Track* track = ball_track();
    ASSERT_NE(track, nullptr);
    EXPECT_EQ(track->state(), tracking::TrackState::Lost);
}

}  // namespace
