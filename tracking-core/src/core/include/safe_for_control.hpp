#pragma once

// TRK-020a/b/c: the ADR-007 safe_for_control predicate. Eight clauses over one
// TrackingSnapshot plus asymmetric hysteresis; the single most safety-critical
// component in the core. Every threshold comparison uses the unsafe-side-wins
// tie rule (plan R11: exactly-at fails); undefined inputs fail their clause
// (plan R12 — ADR-007's "false or undefined").
//
// Hysteresis (plan KTD-8, ADR-007 clarification 2026-07-18): flip-to-false is
// immediate; while unsafe, EVERY evaluation with any failing clause re-anchors
// the dwell, so flip-to-true requires min_unsafe_dwell_ms of continuously
// passing evaluations. Initial state is unsafe with the anchor set when the
// tracker enters RUNNING, so the first flip-to-true always serves a full dwell.

#include <cstdint>

#include "config.hpp"
#include "tracking_snapshot.hpp"

namespace tracking {

class SafeForControlEvaluator {
public:
    SafeForControlEvaluator(const SafeForControlConfig& config, double ball_radius_m);

    // Evaluates the snapshot at `now_ns` (capture clock) and writes the result
    // into snapshot.safety (also returned). Hot path: no allocation, no
    // exceptions. `evaluate` is pure over the snapshot; the only cross-call
    // state is the hysteresis anchor.
    SafetyResult evaluate(TrackingSnapshot& snapshot, std::int64_t now_ns);

private:
    std::uint16_t system_clauses(const TrackingSnapshot& snapshot) const;
    std::uint16_t geometric_clauses(const TrackingSnapshot& snapshot,
                                    SafetyResult& result) const;

    double age_max_ms_ = 0.0;
    double settled_speed_m_per_s_ = 0.0;
    double alignment_tolerance_m_ = 0.0;
    double ball_radius_m_ = 0.0;
    std::int64_t dwell_ns_ = 0;

    // Hysteresis state: anchor of the most recent failing evaluation. Armed at
    // the first evaluation with tracker_state == RUNNING (plan R10).
    std::int64_t last_fail_anchor_ns_ = 0;
    bool anchor_armed_ = false;
    bool currently_safe_ = false;
};

}  // namespace tracking
