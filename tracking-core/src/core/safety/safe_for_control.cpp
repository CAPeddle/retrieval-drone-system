// TRK-020a/b/c: safe_for_control implementation. Clause numbering follows
// ADR-007's Decision section exactly.

#include "safe_for_control.hpp"

#include <cmath>

#include "track.hpp"

namespace tracking {

namespace {

constexpr std::int64_t kNsPerMs = 1'000'000;

bool is_confirmed(const ObjectSlot& slot) {
    return slot.valid &&
           static_cast<TrackState>(slot.track_state) == TrackState::Confirmed;
}

}  // namespace

SafeForControlEvaluator::SafeForControlEvaluator(const SafeForControlConfig& config,
                                                 double ball_radius_m)
    : age_max_ms_(config.age_max_ms),
      settled_speed_m_per_s_(config.laser_settled_speed_m_per_s),
      alignment_tolerance_m_(config.alignment_tolerance_m),
      ball_radius_m_(ball_radius_m),
      dwell_ns_(static_cast<std::int64_t>(
          std::llround(config.min_unsafe_dwell_ms * 1.0e6))) {}

std::uint16_t SafeForControlEvaluator::system_clauses(
    const TrackingSnapshot& snapshot) const {
    std::uint16_t reasons = 0;
    // Clause 1: tracker running.
    if (snapshot.health.tracker_state != TrackerState::Running) {
        reasons |= kTrackerNotRunning;
    }
    // Clause 2: calibration valid (constant Valid in v0.3, KTD-7 — the check
    // still runs so TRK-024's status wiring lands without touching clauses).
    if (snapshot.health.calibration_status != CalibrationStatus::Valid) {
        reasons |= kCalibrationInvalid;
    }
    // Clauses 3/4: Confirmed tracks. An invalid slot (no track, or degenerate
    // floor mapping) is "undefined" and fails (plan R12).
    if (!is_confirmed(snapshot.laser)) {
        reasons |= kLaserNotConfirmed;
    }
    if (!is_confirmed(snapshot.ball)) {
        reasons |= kBallNotConfirmed;
    }
    return reasons;
}

std::uint16_t SafeForControlEvaluator::geometric_clauses(
    const TrackingSnapshot& snapshot, SafetyResult& result) const {
    std::uint16_t reasons = 0;
    // Clauses 5/6: freshness. Ties fail (>=, plan R11); an invalid slot's age
    // is meaningless but clauses 3/4 already failed it — evaluate defensively.
    if (!snapshot.laser.valid || snapshot.laser.age_ms >= age_max_ms_) {
        reasons |= kAgeExceedsThreshold;
    }
    if (!snapshot.ball.valid || snapshot.ball.age_ms >= age_max_ms_) {
        reasons |= kAgeExceedsThreshold;
    }
    // Clause 7: laser settled. Undefined speed (fewer than two observations)
    // fails (plan R12).
    if (!snapshot.laser.valid || !snapshot.laser.speed_valid ||
        snapshot.laser.speed_m_per_s >= settled_speed_m_per_s_) {
        reasons |= kLaserInTransit;
    }
    // Clause 8: alignment, in FloorPlane2D on Z-compensated positions
    // (ADR-006/ADR-010). Distance exposed for the viewer (plan KTD-3).
    if (snapshot.laser.valid && snapshot.ball.valid) {
        const double distance = std::hypot(
            snapshot.laser.floor_x_m - snapshot.ball.floor_x_m,
            snapshot.laser.floor_y_m - snapshot.ball.floor_y_m);
        if (std::isfinite(distance)) {
            result.laser_ball_distance_m = distance;
            result.distance_valid = true;
            if (distance >= ball_radius_m_ + alignment_tolerance_m_) {
                reasons |= kLaserBallMisaligned;
            }
        } else {
            reasons |= kLaserBallMisaligned;  // undefined fails (plan R12)
        }
    } else {
        reasons |= kLaserBallMisaligned;
    }
    return reasons;
}

SafetyResult SafeForControlEvaluator::evaluate(TrackingSnapshot& snapshot,
                                               std::int64_t now_ns) {
    SafetyResult result;

    // Arm the dwell anchor at RUNNING entry (plan R10): the first flip-to-true
    // must serve a full dwell — an unarmed anchor against an arbitrary clock
    // epoch would permit a startup false positive.
    if (!anchor_armed_ && snapshot.health.tracker_state == TrackerState::Running) {
        last_fail_anchor_ns_ = now_ns;
        anchor_armed_ = true;
    }

    result.unsafe_reasons =
        static_cast<std::uint16_t>(system_clauses(snapshot) |
                                   geometric_clauses(snapshot, result));
    const bool all_pass = result.unsafe_reasons == 0 && anchor_armed_;

    if (!all_pass) {
        // Flip-to-false is immediate; every failing evaluation re-anchors the
        // dwell (KTD-8 / ADR-007 clarification) — alternating pass/fail can
        // never accumulate dwell credit.
        currently_safe_ = false;
        if (anchor_armed_) {
            last_fail_anchor_ns_ = now_ns;
        }
        result.safe = false;
        result.hysteresis_remaining_ms = static_cast<std::uint32_t>(dwell_ns_ / kNsPerMs);
        snapshot.safety = result;
        return result;
    }

    const std::int64_t since_fail_ns = now_ns - last_fail_anchor_ns_;
    if (since_fail_ns >= dwell_ns_) {  // dwell served at exactly the boundary
        currently_safe_ = true;
        result.safe = true;
        result.hysteresis_remaining_ms = 0;
    } else {
        result.safe = false;
        result.hysteresis_remaining_ms =
            static_cast<std::uint32_t>((dwell_ns_ - since_fail_ns) / kNsPerMs);
    }
    snapshot.safety = result;
    return result;
}

}  // namespace tracking
