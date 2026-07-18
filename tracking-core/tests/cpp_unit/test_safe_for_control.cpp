// TRK-020a/b/c: safe_for_control predicate tests. One test per clause with
// all-else-passing snapshots, boundary values (ties fail, plan R11), undefined
// inputs (plan R12), and the KTD-8 re-anchor hysteresis (the chatter scenario
// is the load-bearing one: alternating pass/fail must never flip true).

#include "safe_for_control.hpp"

#include <gtest/gtest.h>

#include <cstdint>

#include "track.hpp"

namespace {

using tracking::CalibrationStatus;
using tracking::ObjectSlot;
using tracking::SafeForControlEvaluator;
using tracking::SafetyResult;
using tracking::TrackerState;
using tracking::TrackingSnapshot;
using tracking::TrackState;

constexpr std::int64_t kMs = 1'000'000;

tracking::SafeForControlConfig make_config() {
    tracking::SafeForControlConfig cfg;
    cfg.age_max_ms = 50.0;
    cfg.laser_settled_speed_m_per_s = 0.05;
    cfg.alignment_tolerance_m = 0.02;
    cfg.min_unsafe_dwell_ms = 200.0;
    return cfg;
}

constexpr double kBallRadiusM = 0.03;  // clause-8 envelope: 0.05 m

ObjectSlot confirmed_slot(tracking::ObjectType type, double x_m, double y_m,
                          double speed = 0.01) {
    ObjectSlot slot;
    slot.valid = true;
    slot.object_type = static_cast<std::uint8_t>(type);
    slot.track_state = static_cast<std::uint8_t>(TrackState::Confirmed);
    slot.track_id = type == tracking::ObjectType::Laser ? 1 : 2;
    slot.floor_x_m = x_m;
    slot.floor_y_m = y_m;
    slot.floor_z_m = type == tracking::ObjectType::Ball ? kBallRadiusM : 0.0;
    slot.uncertainty_m = 0.005;
    slot.age_ms = 10.0;
    slot.speed_m_per_s = speed;
    slot.speed_valid = true;
    return slot;
}

// Everything passing: Running, both Confirmed, fresh, settled, aligned
// (distance 0.01 m < 0.05 m envelope).
TrackingSnapshot all_pass_snapshot() {
    TrackingSnapshot snapshot;
    snapshot.health.tracker_state = TrackerState::Running;
    snapshot.health.calibration_status = CalibrationStatus::Valid;
    snapshot.laser = confirmed_slot(tracking::ObjectType::Laser, 0.01, 2.0);
    snapshot.ball = confirmed_slot(tracking::ObjectType::Ball, 0.0, 2.0);
    return snapshot;
}

// Arms the anchor at t=0 and serves a full dwell so a subsequent all-pass
// evaluation flips true at `t >= dwell`.
SafeForControlEvaluator armed_evaluator(TrackingSnapshot& snapshot) {
    SafeForControlEvaluator evaluator(make_config(), kBallRadiusM);
    evaluator.evaluate(snapshot, 0);  // arms anchor at RUNNING entry
    return evaluator;
}

// ── Clause-by-clause: only the target clause fails ───────────────────────────

TEST(SafeForControlClauses, AllPassAfterDwellIsSafe) {
    TrackingSnapshot snapshot = all_pass_snapshot();
    SafeForControlEvaluator evaluator = armed_evaluator(snapshot);
    const SafetyResult result = evaluator.evaluate(snapshot, 250 * kMs);
    EXPECT_TRUE(result.safe);
    EXPECT_EQ(result.unsafe_reasons, 0);
    EXPECT_EQ(result.hysteresis_remaining_ms, 0U);
    EXPECT_TRUE(result.distance_valid);
    EXPECT_NEAR(result.laser_ball_distance_m, 0.01, 1e-9);
}

TEST(SafeForControlClauses, Clause1TrackerNotRunning) {
    TrackingSnapshot snapshot = all_pass_snapshot();
    SafeForControlEvaluator evaluator = armed_evaluator(snapshot);
    snapshot.health.tracker_state = TrackerState::Initialising;
    const SafetyResult result = evaluator.evaluate(snapshot, 250 * kMs);
    EXPECT_FALSE(result.safe);
    EXPECT_TRUE(result.unsafe_reasons & tracking::kTrackerNotRunning);
}

TEST(SafeForControlClauses, Clause3LaserNotConfirmed) {
    TrackingSnapshot snapshot = all_pass_snapshot();
    SafeForControlEvaluator evaluator = armed_evaluator(snapshot);
    snapshot.laser.track_state = static_cast<std::uint8_t>(TrackState::Provisional);
    const SafetyResult result = evaluator.evaluate(snapshot, 250 * kMs);
    EXPECT_FALSE(result.safe);
    EXPECT_TRUE(result.unsafe_reasons & tracking::kLaserNotConfirmed);
    EXPECT_FALSE(result.unsafe_reasons & tracking::kBallNotConfirmed);
}

TEST(SafeForControlClauses, Clause4BallLostOrSlotInvalid) {
    TrackingSnapshot snapshot = all_pass_snapshot();
    SafeForControlEvaluator evaluator = armed_evaluator(snapshot);
    snapshot.ball.track_state = static_cast<std::uint8_t>(TrackState::Lost);
    EXPECT_TRUE(evaluator.evaluate(snapshot, 250 * kMs).unsafe_reasons &
                tracking::kBallNotConfirmed);

    TrackingSnapshot snapshot2 = all_pass_snapshot();
    snapshot2.ball.valid = false;  // undefined input fails (plan R12)
    EXPECT_TRUE(evaluator.evaluate(snapshot2, 260 * kMs).unsafe_reasons &
                tracking::kBallNotConfirmed);
}

TEST(SafeForControlClauses, Clauses5And6AgeBoundaryTiesFail) {
    TrackingSnapshot snapshot = all_pass_snapshot();
    SafeForControlEvaluator evaluator = armed_evaluator(snapshot);

    snapshot.laser.age_ms = 49.999;  // just under: passes
    EXPECT_FALSE(evaluator.evaluate(snapshot, 250 * kMs).unsafe_reasons &
                 tracking::kAgeExceedsThreshold);

    snapshot.laser.age_ms = 50.0;  // exactly at: fails (plan R11)
    EXPECT_TRUE(evaluator.evaluate(snapshot, 251 * kMs).unsafe_reasons &
                tracking::kAgeExceedsThreshold);

    snapshot.laser.age_ms = 10.0;
    snapshot.ball.age_ms = 50.0;  // ball age fails clause 6 with the same reason
    EXPECT_TRUE(evaluator.evaluate(snapshot, 252 * kMs).unsafe_reasons &
                tracking::kAgeExceedsThreshold);
}

TEST(SafeForControlClauses, Clause7SpeedBoundaryAndUndefined) {
    TrackingSnapshot snapshot = all_pass_snapshot();
    SafeForControlEvaluator evaluator = armed_evaluator(snapshot);

    snapshot.laser.speed_m_per_s = 0.0499;  // just under: passes
    EXPECT_FALSE(evaluator.evaluate(snapshot, 250 * kMs).unsafe_reasons &
                 tracking::kLaserInTransit);

    snapshot.laser.speed_m_per_s = 0.05;  // exactly at: fails
    EXPECT_TRUE(evaluator.evaluate(snapshot, 251 * kMs).unsafe_reasons &
                tracking::kLaserInTransit);

    snapshot.laser.speed_m_per_s = 0.01;
    snapshot.laser.speed_valid = false;  // undefined fails (plan R12)
    EXPECT_TRUE(evaluator.evaluate(snapshot, 252 * kMs).unsafe_reasons &
                tracking::kLaserInTransit);
}

TEST(SafeForControlClauses, Clause8DistanceBoundary) {
    TrackingSnapshot snapshot = all_pass_snapshot();
    SafeForControlEvaluator evaluator = armed_evaluator(snapshot);

    snapshot.laser.floor_x_m = snapshot.ball.floor_x_m + 0.0499;  // just inside
    const SafetyResult inside = evaluator.evaluate(snapshot, 250 * kMs);
    EXPECT_FALSE(inside.unsafe_reasons & tracking::kLaserBallMisaligned);

    snapshot.laser.floor_x_m = snapshot.ball.floor_x_m + 0.05;  // exactly at: fails
    const SafetyResult at = evaluator.evaluate(snapshot, 251 * kMs);
    EXPECT_TRUE(at.unsafe_reasons & tracking::kLaserBallMisaligned);
    EXPECT_TRUE(at.distance_valid);
    EXPECT_NEAR(at.laser_ball_distance_m, 0.05, 1e-9);
}

TEST(SafeForControlClauses, CombinedFailuresAccumulateReasons) {
    TrackingSnapshot snapshot = all_pass_snapshot();
    SafeForControlEvaluator evaluator = armed_evaluator(snapshot);
    snapshot.health.tracker_state = TrackerState::Initialising;
    snapshot.laser.speed_m_per_s = 1.0;
    snapshot.laser.floor_x_m = 5.0;  // NOTE: outside envelope
    const SafetyResult result = evaluator.evaluate(snapshot, 250 * kMs);
    EXPECT_TRUE(result.unsafe_reasons & tracking::kTrackerNotRunning);
    EXPECT_TRUE(result.unsafe_reasons & tracking::kLaserInTransit);
    EXPECT_TRUE(result.unsafe_reasons & tracking::kLaserBallMisaligned);
}

// ── Hysteresis (KTD-8 / ADR-007 clarification) ───────────────────────────────

TEST(SafeForControlHysteresis, FirstEverPassWithinDwellStaysFalse) {
    // Startup false-positive guard (plan R10): the anchor arms at RUNNING
    // entry, so an immediately-passing snapshot still serves the full dwell.
    TrackingSnapshot snapshot = all_pass_snapshot();
    SafeForControlEvaluator evaluator(make_config(), kBallRadiusM);
    const SafetyResult first = evaluator.evaluate(snapshot, 0);
    EXPECT_FALSE(first.safe);
    EXPECT_EQ(first.hysteresis_remaining_ms, 200U);

    EXPECT_FALSE(evaluator.evaluate(snapshot, 199 * kMs).safe);
    EXPECT_TRUE(evaluator.evaluate(snapshot, 200 * kMs).safe);  // dwell served at boundary
}

TEST(SafeForControlHysteresis, FlipToFalseIsImmediate) {
    TrackingSnapshot snapshot = all_pass_snapshot();
    SafeForControlEvaluator evaluator = armed_evaluator(snapshot);
    ASSERT_TRUE(evaluator.evaluate(snapshot, 250 * kMs).safe);

    snapshot.laser.age_ms = 60.0;  // clause 5 fails
    EXPECT_FALSE(evaluator.evaluate(snapshot, 251 * kMs).safe);
}

TEST(SafeForControlHysteresis, ChatterNeverFlipsTrue) {
    // Alternating pass/fail at frame rate: under the re-anchor rule the dwell
    // never accumulates, so safe never flips true — the scenario the KTD-8
    // literal-ADR reading would have permitted every dwell period.
    TrackingSnapshot pass = all_pass_snapshot();
    TrackingSnapshot fail = all_pass_snapshot();
    fail.laser.age_ms = 60.0;

    SafeForControlEvaluator evaluator(make_config(), kBallRadiusM);
    std::int64_t t = 0;
    for (int i = 0; i < 200; ++i) {  // 200 frames ~ 3.3 s at 60 fps
        TrackingSnapshot& snapshot = (i % 2 == 0) ? fail : pass;
        const SafetyResult result = evaluator.evaluate(snapshot, t);
        EXPECT_FALSE(result.safe) << "flipped true at frame " << i;
        t += 16 * kMs;
    }
}

TEST(SafeForControlHysteresis, MidDwellFailureReanchors) {
    TrackingSnapshot pass = all_pass_snapshot();
    TrackingSnapshot fail = all_pass_snapshot();
    fail.laser.age_ms = 60.0;

    SafeForControlEvaluator evaluator(make_config(), kBallRadiusM);
    evaluator.evaluate(fail, 0);              // anchor at 0
    evaluator.evaluate(pass, 100 * kMs);      // 100 ms into the dwell
    evaluator.evaluate(fail, 120 * kMs);      // re-anchor at 120 ms
    // 200 ms after the ORIGINAL anchor but only 90 ms after the re-anchor:
    EXPECT_FALSE(evaluator.evaluate(pass, 210 * kMs).safe);
    // Full dwell after the re-anchor:
    EXPECT_TRUE(evaluator.evaluate(pass, 320 * kMs).safe);
}

TEST(SafeForControlHysteresis, RemainingMsCountsDown) {
    TrackingSnapshot snapshot = all_pass_snapshot();
    SafeForControlEvaluator evaluator(make_config(), kBallRadiusM);
    evaluator.evaluate(snapshot, 0);
    EXPECT_EQ(evaluator.evaluate(snapshot, 50 * kMs).hysteresis_remaining_ms, 150U);
    EXPECT_EQ(evaluator.evaluate(snapshot, 150 * kMs).hysteresis_remaining_ms, 50U);
    EXPECT_EQ(evaluator.evaluate(snapshot, 200 * kMs).hysteresis_remaining_ms, 0U);
}

TEST(SafeForControlHysteresis, NotArmedBeforeRunningNeverSafe) {
    TrackingSnapshot snapshot = all_pass_snapshot();
    snapshot.health.tracker_state = TrackerState::Initialising;
    SafeForControlEvaluator evaluator(make_config(), kBallRadiusM);
    for (std::int64_t t = 0; t < 1000 * kMs; t += 100 * kMs) {
        EXPECT_FALSE(evaluator.evaluate(snapshot, t).safe);
    }
}

}  // namespace
