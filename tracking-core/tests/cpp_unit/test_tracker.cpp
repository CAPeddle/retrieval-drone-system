// TRK-015: observation gating and association tests. Type-keyed gates (plan
// R2), nearest-wins contention, slot release at Lost (plan R3), and Retired
// removal.

#include "tracker.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

using tracking::GatingConfig;
using tracking::Observation;
using tracking::ObjectType;
using tracking::Track;
using tracking::TrackConfig;
using tracking::Tracker;
using tracking::TrackState;

constexpr std::int64_t kMs = 1'000'000;

TrackConfig make_track_config() {
    TrackConfig cfg;
    cfg.confirm_threshold = 3;
    cfg.predict_timeout_ms = 50.0;
    cfg.occlude_timeout_ms = 200.0;
    cfg.retire_timeout_ms = 1000.0;
    cfg.max_predict_duration_ms = 100.0;
    return cfg;
}

GatingConfig make_gating_config() {
    GatingConfig cfg;
    cfg.max_distance_px = 50.0;
    return cfg;
}

Observation make_obs(ObjectType type, float x, float y, std::int64_t t_ns) {
    Observation obs;
    obs.type = type;
    obs.centroid_px = {x, y};
    obs.radius_px = 12.0;
    obs.capture_timestamp_ns = t_ns;
    return obs;
}

class TrackerTest : public ::testing::Test {
protected:
    Tracker tracker_{make_track_config(), make_gating_config()};

    // Runs one cycle with the given observations at time t.
    const std::vector<Track>& cycle(std::vector<Observation> observations,
                                    std::int64_t t_ns) {
        return tracker_.update(observations, t_ns);
    }

    // Spawns and confirms a ball track near (x, y); returns its ID.
    std::uint32_t confirm_ball(float x, float y, std::int64_t start_ns) {
        for (int i = 0; i < 3; ++i) {
            cycle({make_obs(ObjectType::Ball, x, y, start_ns + i * 10 * kMs)},
                  start_ns + i * 10 * kMs);
        }
        EXPECT_EQ(tracker_.tracks().size(), 1U);
        EXPECT_EQ(tracker_.tracks()[0].state(), TrackState::Confirmed);
        return tracker_.tracks()[0].id();
    }
};

TEST_F(TrackerTest, FirstObservationSpawnsProvisional) {
    const auto& tracks = cycle({make_obs(ObjectType::Ball, 100, 100, 0)}, 0);
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_EQ(tracks[0].state(), TrackState::Provisional);
    EXPECT_EQ(tracks[0].type(), ObjectType::Ball);
}

TEST_F(TrackerTest, NearObservationAssociates) {
    const std::uint32_t id = confirm_ball(100, 100, 0);
    // 30 px away: inside the 50 px gate — same track, updated position.
    const auto& tracks = cycle({make_obs(ObjectType::Ball, 130, 100, 40 * kMs)}, 40 * kMs);
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_EQ(tracks[0].id(), id);
    EXPECT_FLOAT_EQ(tracks[0].position_px().x, 130.0F);
    EXPECT_EQ(tracks[0].state(), TrackState::Confirmed);
}

TEST_F(TrackerTest, FarObservationDoesNotSpawnWhenSlotOccupied) {
    confirm_ball(100, 100, 0);
    // 300 px away: outside the gate. The single ball slot is occupied by a
    // live track, so no second ball track spawns; the existing one decays.
    const auto& tracks = cycle({make_obs(ObjectType::Ball, 400, 100, 40 * kMs)}, 40 * kMs);
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_EQ(tracks[0].state(), TrackState::Predicted);
}

TEST_F(TrackerTest, ExactlyAtGateDoesNotAssociate) {
    // Gate is strict <: an observation at exactly max_distance_px spawns
    // nothing (slot occupied) rather than associating.
    confirm_ball(100, 100, 0);
    const auto& tracks = cycle({make_obs(ObjectType::Ball, 150, 100, 40 * kMs)}, 40 * kMs);
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_EQ(tracks[0].state(), TrackState::Predicted);  // not observed
}

TEST_F(TrackerTest, BallObservationNeverFeedsLaserTrack) {
    // The false-SAFE path from flow analysis (plan R2): a laser track exists
    // and a ball observation lands on top of it. It must spawn a ball track,
    // never feed the laser track — even though it is nearest.
    for (int i = 0; i < 3; ++i) {
        cycle({make_obs(ObjectType::Laser, 100, 100, i * 10 * kMs)}, i * 10 * kMs);
    }
    ASSERT_EQ(tracker_.tracks().size(), 1U);
    ASSERT_EQ(tracker_.tracks()[0].type(), ObjectType::Laser);
    const int laser_count = tracker_.tracks()[0].observation_count();

    const auto& tracks =
        cycle({make_obs(ObjectType::Ball, 101, 100, 30 * kMs)}, 30 * kMs);
    ASSERT_EQ(tracks.size(), 2U);
    const Track& laser = tracks[0].type() == ObjectType::Laser ? tracks[0] : tracks[1];
    const Track& ball = tracks[0].type() == ObjectType::Ball ? tracks[0] : tracks[1];
    EXPECT_EQ(laser.observation_count(), laser_count);  // untouched
    EXPECT_EQ(ball.state(), TrackState::Provisional);
}

TEST_F(TrackerTest, NearerOfTwoSameTypeObservationsWins) {
    const std::uint32_t id = confirm_ball(100, 100, 0);
    // Two ball observations inside the gate; the nearer (110) must own the
    // track. The farther one cannot spawn (single ball slot occupied).
    const auto& tracks = cycle({make_obs(ObjectType::Ball, 140, 100, 40 * kMs),
                                make_obs(ObjectType::Ball, 110, 100, 40 * kMs)},
                               40 * kMs);
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_EQ(tracks[0].id(), id);
    EXPECT_FLOAT_EQ(tracks[0].position_px().x, 110.0F);
}

TEST_F(TrackerTest, UnobservedTrackAges) {
    confirm_ball(100, 100, 0);
    const std::int64_t last = tracker_.tracks()[0].last_observation_ns();
    cycle({}, last + 60 * kMs);
    EXPECT_EQ(tracker_.tracks()[0].state(), TrackState::Occluded);
}

TEST_F(TrackerTest, LostTrackReleasesSlotAndIgnoresObservations) {
    const std::uint32_t old_id = confirm_ball(100, 100, 0);
    const std::int64_t last = tracker_.tracks()[0].last_observation_ns();

    // Age past occlude_timeout: Lost. Slot released (plan R3).
    cycle({}, last + 300 * kMs);
    ASSERT_EQ(tracker_.tracks()[0].state(), TrackState::Lost);

    // A new observation right where the Lost track sits spawns a FRESH
    // Provisional immediately — no 1 s lockout, no feeding the Lost track.
    const auto& tracks =
        cycle({make_obs(ObjectType::Ball, 100, 100, last + 310 * kMs)}, last + 310 * kMs);
    ASSERT_EQ(tracks.size(), 2U);
    const Track& fresh = tracks[0].id() == old_id ? tracks[1] : tracks[0];
    EXPECT_NE(fresh.id(), old_id);
    EXPECT_EQ(fresh.state(), TrackState::Provisional);
}

TEST_F(TrackerTest, RetiredTracksAreRemoved) {
    confirm_ball(100, 100, 0);
    const std::int64_t last = tracker_.tracks()[0].last_observation_ns();
    cycle({}, last + 2000 * kMs);  // past retire_timeout: cascades to Retired
    EXPECT_TRUE(tracker_.tracks().empty());
}

TEST_F(TrackerTest, NewTrackIdsNeverReuseOldOnes) {
    const std::uint32_t first = confirm_ball(100, 100, 0);
    const std::int64_t last = tracker_.tracks()[0].last_observation_ns();
    cycle({}, last + 2000 * kMs);
    ASSERT_TRUE(tracker_.tracks().empty());

    const auto& tracks =
        cycle({make_obs(ObjectType::Ball, 200, 200, last + 2010 * kMs)}, last + 2010 * kMs);
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_GT(tracks[0].id(), first);
}

TEST_F(TrackerTest, MarkerTypeAllowsMultipleTracks) {
    const auto& tracks = cycle({make_obs(ObjectType::CalibrationMarker, 100, 100, 0),
                                make_obs(ObjectType::CalibrationMarker, 400, 100, 0)},
                               0);
    EXPECT_EQ(tracks.size(), 2U);  // per-type limit is >1 for markers
}

}  // namespace
