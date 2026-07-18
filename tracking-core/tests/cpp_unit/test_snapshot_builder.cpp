// TRK-020 / plan U8: snapshot assembly tests — observation-backed speed with
// dt guards, finite sanitisation, INITIALISING -> RUNNING, trivially-copyable
// contract (compile-time in tracking_snapshot.hpp).

#include "snapshot_builder.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "calibration_fixture.hpp"

namespace {

namespace fx = calibration_fixture;

using tracking::CalibrationArtifacts;
using tracking::CoordinateMapper;
using tracking::Observation;
using tracking::ObjectType;
using tracking::SnapshotBuilder;
using tracking::Track;
using tracking::TrackConfig;
using tracking::TrackerState;
using tracking::TrackingSnapshot;

constexpr std::int64_t kMs = 1'000'000;

CalibrationArtifacts artifacts() {
    CalibrationArtifacts a;
    a.intrinsics.camera_matrix = fx::camera_matrix();
    a.intrinsics.dist_coeffs = {0.0, 0.0, 0.0, 0.0, 0.0};
    a.intrinsics.image_width = 640;
    a.intrinsics.image_height = 480;
    a.extrinsics.homography = fx::pixel_to_floor();
    return a;
}

TrackConfig track_config() {
    TrackConfig cfg;
    cfg.confirm_threshold = 3;
    cfg.predict_timeout_ms = 50.0;
    cfg.occlude_timeout_ms = 200.0;
    cfg.retire_timeout_ms = 1000.0;
    cfg.max_predict_duration_ms = 100.0;
    return cfg;
}

Observation ball_obs_at_floor(double x_m, double y_m, std::int64_t t_ns) {
    const cv::Point2d px = fx::pixel_of_world({x_m, y_m, 0.03});
    Observation obs;
    obs.type = ObjectType::Ball;
    obs.centroid_px = {static_cast<float>(px.x), static_cast<float>(px.y)};
    obs.radius_px = 15.0;
    obs.capture_timestamp_ns = t_ns;
    return obs;
}

class SnapshotBuilderTest : public ::testing::Test {
protected:
    CalibrationArtifacts artifacts_ = artifacts();
    CoordinateMapper mapper_{artifacts_, fx::coordinate_config(), 0.03};
    SnapshotBuilder builder_{mapper_, fx::coordinate_config()};
};

TEST_F(SnapshotBuilderTest, RunningTransitionFiresOnceOnAdmittedFrame) {
    std::vector<Track> tracks;
    TrackingSnapshot before = builder_.build(tracks, 0, 0);
    EXPECT_EQ(before.health.tracker_state, TrackerState::Initialising);

    builder_.mark_frame_admitted();
    TrackingSnapshot after = builder_.build(tracks, 1, 1);
    EXPECT_EQ(after.health.tracker_state, TrackerState::Running);
}

TEST_F(SnapshotBuilderTest, BallSlotCarriesZCompensatedFloorPosition) {
    std::vector<Track> tracks;
    tracks.emplace_back(1, ball_obs_at_floor(0.0, 2.0, 0), track_config());
    tracks[0].tick(true, 0);

    const TrackingSnapshot snapshot = builder_.build(tracks, 0, 5 * kMs);
    ASSERT_TRUE(snapshot.ball.valid);
    EXPECT_NEAR(snapshot.ball.floor_x_m, 0.0, 0.005);  // R6 budget
    EXPECT_NEAR(snapshot.ball.floor_y_m, 2.0, 0.005);
    EXPECT_DOUBLE_EQ(snapshot.ball.floor_z_m, 0.03);
    EXPECT_NEAR(snapshot.ball.age_ms, 5.0, 1e-6);
    EXPECT_FALSE(snapshot.ball.speed_valid);  // one observation: undefined (R12)
    EXPECT_FALSE(snapshot.laser.valid);       // no laser track this frame
}

TEST_F(SnapshotBuilderTest, SpeedFromConsecutiveObservations) {
    std::vector<Track> tracks;
    tracks.emplace_back(1, ball_obs_at_floor(0.0, 2.0, 0), track_config());
    tracks[0].tick(true, 0);
    builder_.build(tracks, 0, 0);  // first sample

    // 0.1 m in 100 ms -> 1.0 m/s.
    tracks[0].observe(ball_obs_at_floor(0.1, 2.0, 100 * kMs));
    tracks[0].tick(true, 100 * kMs);
    const TrackingSnapshot snapshot = builder_.build(tracks, 100 * kMs, 100 * kMs);
    ASSERT_TRUE(snapshot.ball.valid);
    ASSERT_TRUE(snapshot.ball.speed_valid);
    EXPECT_NEAR(snapshot.ball.speed_m_per_s, 1.0, 0.15);  // mapping tolerance
}

TEST_F(SnapshotBuilderTest, PredictedFramesDoNotUpdateSpeed) {
    std::vector<Track> tracks;
    tracks.emplace_back(1, ball_obs_at_floor(0.0, 2.0, 0), track_config());
    tracks[0].tick(true, 0);
    builder_.build(tracks, 0, 0);
    tracks[0].observe(ball_obs_at_floor(0.05, 2.0, 50 * kMs));
    tracks[0].tick(true, 50 * kMs);
    const double speed_before =
        builder_.build(tracks, 50 * kMs, 50 * kMs).ball.speed_m_per_s;

    // Coasting frames: position extrapolates, but no new observation arrives —
    // speed must stay at the last observation-backed value.
    for (int i = 1; i <= 3; ++i) {
        tracks[0].tick(false, (50 + i * 16) * kMs);
        const TrackingSnapshot snapshot =
            builder_.build(tracks, (50 + i * 16) * kMs, (50 + i * 16) * kMs);
        EXPECT_NEAR(snapshot.ball.speed_m_per_s, speed_before, 1e-9);
    }
}

TEST_F(SnapshotBuilderTest, NewTrackIdResetsSpeedHistory) {
    std::vector<Track> tracks;
    tracks.emplace_back(7, ball_obs_at_floor(0.0, 2.0, 0), track_config());
    tracks[0].tick(true, 0);
    builder_.build(tracks, 0, 0);
    tracks[0].observe(ball_obs_at_floor(0.1, 2.0, 100 * kMs));
    tracks[0].tick(true, 100 * kMs);
    ASSERT_TRUE(builder_.build(tracks, 100 * kMs, 100 * kMs).ball.speed_valid);

    // Replacement track (new id) at a far position: speed must NOT be computed
    // across the identity change.
    tracks.clear();
    tracks.emplace_back(8, ball_obs_at_floor(0.5, 1.0, 200 * kMs), track_config());
    tracks[0].tick(true, 200 * kMs);
    const TrackingSnapshot snapshot = builder_.build(tracks, 200 * kMs, 200 * kMs);
    EXPECT_FALSE(snapshot.ball.speed_valid);
}

TEST_F(SnapshotBuilderTest, DegenerateMappingInvalidatesSlotNotNaN) {
    // A track parked outside the floor AOI: projection invalid -> slot invalid.
    std::vector<Track> tracks;
    const cv::Point2d px = fx::pixel_of_world({0.0, 5.0, 0.03});  // AOI y_max 3.5
    Observation obs;
    obs.type = ObjectType::Ball;
    obs.centroid_px = {static_cast<float>(px.x), static_cast<float>(px.y)};
    obs.radius_px = 15.0;
    obs.capture_timestamp_ns = 0;
    tracks.emplace_back(1, obs, track_config());
    tracks[0].tick(true, 0);

    const TrackingSnapshot snapshot = builder_.build(tracks, 0, 0);
    EXPECT_FALSE(snapshot.ball.valid);
    EXPECT_TRUE(std::isfinite(snapshot.ball.floor_x_m));  // safe defaults
}

TEST_F(SnapshotBuilderTest, FrameCountersFlowThrough) {
    builder_.note_frame_processed();
    builder_.note_frame_processed();
    builder_.note_frame_rejected();
    const TrackingSnapshot snapshot = builder_.build({}, 0, 0);
    EXPECT_EQ(snapshot.health.frames_processed, 2U);
    EXPECT_EQ(snapshot.health.frames_rejected, 1U);
}

}  // namespace
