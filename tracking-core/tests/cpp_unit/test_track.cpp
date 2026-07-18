// TRK-014: track lifecycle state machine tests. Every transition edge, timer
// boundary (ties decay, plan R11), the Provisional miss limit, Lost slot
// semantics, and ID monotonicity.

#include "track.hpp"

#include <gtest/gtest.h>

#include <cstdint>

namespace {

using tracking::Observation;
using tracking::ObjectType;
using tracking::Track;
using tracking::TrackConfig;
using tracking::TrackIdGenerator;
using tracking::TrackState;

constexpr std::int64_t kMs = 1'000'000;  // ns per ms

TrackConfig make_config() {
    TrackConfig cfg;
    cfg.confirm_threshold = 3;
    cfg.predict_timeout_ms = 50.0;
    cfg.occlude_timeout_ms = 200.0;
    cfg.retire_timeout_ms = 1000.0;
    cfg.max_predict_duration_ms = 100.0;
    return cfg;
}

Observation make_obs(std::int64_t t_ns, float x = 100.0F, float y = 100.0F,
                     ObjectType type = ObjectType::Ball) {
    Observation obs;
    obs.type = type;
    obs.centroid_px = {x, y};
    obs.radius_px = 12.0;
    obs.capture_timestamp_ns = t_ns;
    return obs;
}

// Drives a track to Confirmed with the default config (3 observations).
Track make_confirmed(std::int64_t start_ns = 0) {
    Track track(1, make_obs(start_ns), make_config());
    track.tick(true, start_ns);
    for (int i = 1; i < 3; ++i) {
        const std::int64_t t = start_ns + i * 10 * kMs;
        track.observe(make_obs(t));
        track.tick(true, t);
    }
    EXPECT_EQ(track.state(), TrackState::Confirmed);
    return track;
}

// ── Birth and confirmation ───────────────────────────────────────────────────

TEST(TrackTest, BornProvisionalFromFirstObservation) {
    const Track track(7, make_obs(0), make_config());
    EXPECT_EQ(track.state(), TrackState::Provisional);
    EXPECT_EQ(track.id(), 7U);
    EXPECT_EQ(track.observation_count(), 1);
    EXPECT_EQ(track.type(), ObjectType::Ball);
}

TEST(TrackTest, ConfirmsAtThresholdNotBefore) {
    Track track(1, make_obs(0), make_config());
    track.tick(true, 0);
    EXPECT_EQ(track.state(), TrackState::Provisional);  // count 1 of 3

    track.observe(make_obs(10 * kMs));
    track.tick(true, 10 * kMs);
    EXPECT_EQ(track.state(), TrackState::Provisional);  // count 2 of 3

    track.observe(make_obs(20 * kMs));
    track.tick(true, 20 * kMs);
    EXPECT_EQ(track.state(), TrackState::Confirmed);    // count 3 of 3
}

TEST(TrackTest, ProvisionalDeletedAfterExactlyTwoMissedCycles) {
    Track track(1, make_obs(0), make_config());
    track.tick(false, 5 * kMs);
    EXPECT_EQ(track.state(), TrackState::Provisional);  // 1 missed: still alive
    track.tick(false, 10 * kMs);
    EXPECT_EQ(track.state(), TrackState::Retired);      // 2 missed: deleted
}

TEST(TrackTest, ProvisionalMissCounterResetsOnObservation) {
    Track track(1, make_obs(0), make_config());
    track.tick(false, 5 * kMs);                          // 1 missed
    track.observe(make_obs(10 * kMs));
    track.tick(true, 10 * kMs);                          // reset
    track.tick(false, 15 * kMs);                         // 1 missed again
    EXPECT_EQ(track.state(), TrackState::Provisional);
}

// ── Decay chain and timer boundaries (ties decay, R11) ───────────────────────

TEST(TrackTest, ConfirmedToPredictedOnFirstMissedCycle) {
    Track track = make_confirmed();
    const std::int64_t last = track.last_observation_ns();
    track.tick(false, last + kMs);  // well under predict_timeout
    EXPECT_EQ(track.state(), TrackState::Predicted);
}

TEST(TrackTest, PredictedHoldsJustUnderTimeoutAndDecaysExactlyAt) {
    Track track = make_confirmed();
    const std::int64_t last = track.last_observation_ns();

    track.tick(false, last + 50 * kMs - 1);  // one ns under: holds
    EXPECT_EQ(track.state(), TrackState::Predicted);

    track.tick(false, last + 50 * kMs);      // exactly at: decays (R11)
    EXPECT_EQ(track.state(), TrackState::Occluded);
}

TEST(TrackTest, OccludedToLostAtOccludeTimeout) {
    Track track = make_confirmed();
    const std::int64_t last = track.last_observation_ns();
    track.tick(false, last + 60 * kMs);
    ASSERT_EQ(track.state(), TrackState::Occluded);

    track.tick(false, last + 200 * kMs - 1);
    EXPECT_EQ(track.state(), TrackState::Occluded);      // just under: holds

    track.tick(false, last + 200 * kMs);
    EXPECT_EQ(track.state(), TrackState::Lost);          // exactly at: decays
}

TEST(TrackTest, LostToRetiredAtRetireTimeout) {
    Track track = make_confirmed();
    const std::int64_t last = track.last_observation_ns();
    track.tick(false, last + 300 * kMs);
    ASSERT_EQ(track.state(), TrackState::Lost);

    track.tick(false, last + 1000 * kMs - 1);
    EXPECT_EQ(track.state(), TrackState::Lost);

    track.tick(false, last + 1000 * kMs);
    EXPECT_EQ(track.state(), TrackState::Retired);
}

TEST(TrackTest, LongGapCascadesThroughStatesInOneTick) {
    // A REJECT burst longer than every timeout must age the track all the way
    // (plan R4), not freeze it at Confirmed.
    Track track = make_confirmed();
    const std::int64_t last = track.last_observation_ns();
    track.tick(false, last + 2000 * kMs);
    EXPECT_EQ(track.state(), TrackState::Retired);
}

// ── Re-entry policy (R3) ─────────────────────────────────────────────────────

TEST(TrackTest, PredictedRestoresConfirmedOnOneObservation) {
    Track track = make_confirmed();
    const std::int64_t last = track.last_observation_ns();
    track.tick(false, last + kMs);
    ASSERT_EQ(track.state(), TrackState::Predicted);

    track.observe(make_obs(last + 2 * kMs));
    track.tick(true, last + 2 * kMs);
    EXPECT_EQ(track.state(), TrackState::Confirmed);
}

TEST(TrackTest, OccludedRestoresConfirmedOnOneObservation) {
    Track track = make_confirmed();
    const std::int64_t last = track.last_observation_ns();
    track.tick(false, last + 60 * kMs);
    ASSERT_EQ(track.state(), TrackState::Occluded);

    track.observe(make_obs(last + 61 * kMs));
    track.tick(true, last + 61 * kMs);
    EXPECT_EQ(track.state(), TrackState::Confirmed);
}

TEST(TrackTest, LostIgnoresObservations) {
    Track track = make_confirmed();
    const std::int64_t last = track.last_observation_ns();
    track.tick(false, last + 300 * kMs);
    ASSERT_EQ(track.state(), TrackState::Lost);
    EXPECT_FALSE(track.accepts_observations());

    const int count_before = track.observation_count();
    track.observe(make_obs(last + 301 * kMs));  // defence in depth: no-op
    EXPECT_EQ(track.observation_count(), count_before);
    EXPECT_EQ(track.state(), TrackState::Lost);
}

TEST(TrackTest, RetiredIsTerminal) {
    Track track = make_confirmed();
    const std::int64_t last = track.last_observation_ns();
    track.tick(false, last + 2000 * kMs);
    ASSERT_EQ(track.state(), TrackState::Retired);

    track.observe(make_obs(last + 2001 * kMs));
    track.tick(true, last + 2001 * kMs);
    EXPECT_EQ(track.state(), TrackState::Retired);
    track.tick(false, last + 5000 * kMs);
    EXPECT_EQ(track.state(), TrackState::Retired);
}

// ── Age reporting and state during observation ───────────────────────────────

TEST(TrackTest, AgeReportsMsSinceLastObservationAndClampsAtZero) {
    Track track = make_confirmed(0);
    const std::int64_t last = track.last_observation_ns();
    EXPECT_DOUBLE_EQ(track.age_since_last_observation_ms(last + 25 * kMs), 25.0);
    EXPECT_DOUBLE_EQ(track.age_since_last_observation_ms(last), 0.0);
    EXPECT_DOUBLE_EQ(track.age_since_last_observation_ms(last - kMs), 0.0);
}

TEST(TrackTest, ObservationUpdatesPositionAndRadius) {
    Track track(1, make_obs(0, 100.0F, 100.0F), make_config());
    track.tick(true, 0);
    track.observe(make_obs(10 * kMs, 140.0F, 90.0F));
    EXPECT_FLOAT_EQ(track.position_px().x, 140.0F);
    EXPECT_FLOAT_EQ(track.position_px().y, 90.0F);
    EXPECT_EQ(track.last_observation_ns(), 10 * kMs);
}

TEST(TrackTest, ConfirmedStaysConfirmedWhileObserved) {
    Track track = make_confirmed();
    for (int i = 1; i <= 10; ++i) {
        const std::int64_t t = track.last_observation_ns() + 16 * kMs;
        track.observe(make_obs(t));
        track.tick(true, t);
        EXPECT_EQ(track.state(), TrackState::Confirmed);
    }
}

// ── Parallel tracks and IDs ──────────────────────────────────────────────────

TEST(TrackTest, ParallelTracksAgeIndependently) {
    Track a = make_confirmed(0);
    Track b = make_confirmed(0);

    // a keeps being observed; b decays.
    const std::int64_t t1 = a.last_observation_ns() + 60 * kMs;
    a.observe(make_obs(t1));
    a.tick(true, t1);
    b.tick(false, t1);

    EXPECT_EQ(a.state(), TrackState::Confirmed);
    EXPECT_EQ(b.state(), TrackState::Occluded);
}

TEST(TrackIdGeneratorTest, IdsStrictlyIncreaseAndStartAtOne) {
    TrackIdGenerator gen;
    const std::uint32_t first = gen.allocate();
    EXPECT_EQ(first, 1U);
    std::uint32_t prev = first;
    for (int i = 0; i < 100; ++i) {
        const std::uint32_t next = gen.allocate();
        EXPECT_GT(next, prev);  // never reused, monotonic
        prev = next;
    }
}

}  // namespace
