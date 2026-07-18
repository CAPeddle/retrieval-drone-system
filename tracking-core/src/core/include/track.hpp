#pragma once

// TRK-014: track lifecycle state machine. Every tracked object (laser, ball,
// calibration marker) moves through Provisional -> Confirmed -> Predicted ->
// Occluded -> Lost -> Retired, driven by observation arrivals and capture-clock
// timers. The safe_for_control predicate (ADR-007 clauses 3-4) admits only
// Confirmed tracks, so the transition rules here are safety-load-bearing.
//
// Tie-break rule (plan R11): every timer comparison uses >=, so an age exactly
// at a threshold DECAYS — the unsafe/decayed side wins ties. All timers derive
// from steady_clock capture timestamps (plan R4), never wall clock.

#include <cstdint>
#include <type_traits>

#include <opencv2/core.hpp>

#include "config.hpp"

namespace tracking {

enum class ObjectType : std::uint8_t { Laser, Ball, CalibrationMarker };

enum class TrackState : std::uint8_t {
    Provisional,  // seen, not yet trusted (observation_count < confirm_threshold)
    Confirmed,    // trusted; the only state the ADR-007 predicate accepts
    Predicted,    // missed >= 1 cycle; position extrapolated (TRK-016)
    Occluded,     // still coasting, past predict_timeout_ms
    Lost,         // no longer accepting observations; type slot released (plan R3)
    Retired,      // removed from the active set; IDs are never reused
};

// A single detector output normalised for the tracker. Built at the main-loop
// observation seam (plan U4) from BallObservation / MarkerObservation; the
// synthetic-laser injection used by tests enters through this same type (KTD-6).
struct Observation {
    ObjectType type = ObjectType::Ball;
    cv::Point2f centroid_px{0.0F, 0.0F};
    double radius_px = 0.0;
    std::int64_t capture_timestamp_ns = 0;  // steady_clock, from FrameMetadata
};
static_assert(std::is_trivially_copyable_v<Observation>,
              "Observation is copied per frame on the hot path");

// Monotonic track-ID source. IDs start at 1 (0 reserved for "no track") and are
// never reused within a process, including across retire/create cycles.
class TrackIdGenerator {
public:
    std::uint32_t allocate() noexcept { return next_++; }

private:
    std::uint32_t next_ = 1;
};

class Track {
public:
    // A track is born Provisional from its first observation. Timeouts arrive
    // via TrackConfig (milliseconds in config; converted to ns once here).
    Track(std::uint32_t id, const Observation& first, const TrackConfig& config);

    // Records an associated observation. Provisional counts toward
    // confirmation; Predicted/Occluded return to Confirmed on the next tick;
    // Lost/Retired ignore observations (association must not route here — this
    // is defence in depth, see plan R3).
    void observe(const Observation& observation);

    // Advances the state machine one processing cycle. `observed` says whether
    // observe() was called since the previous tick. Decay transitions cascade
    // within one tick (a long REJECT burst may take Confirmed -> Predicted ->
    // Occluded -> Lost in order, per plan R4) but never skip a state.
    void tick(bool observed, std::int64_t now_ns);

    std::uint32_t id() const noexcept { return id_; }
    ObjectType type() const noexcept { return type_; }
    TrackState state() const noexcept { return state_; }
    int observation_count() const noexcept { return observation_count_; }
    cv::Point2f position_px() const noexcept { return position_px_; }
    double radius_px() const noexcept { return radius_px_; }

    // TRK-016 constant-velocity prediction. Velocity is estimated from the
    // last two observations (observation-backed only — predicted positions
    // never feed it); valid once two distinct-timestamp observations exist.
    bool has_velocity() const noexcept { return has_velocity_; }
    cv::Point2f velocity_px_per_s() const noexcept { return velocity_px_per_s_; }

    // Additional positional uncertainty from prediction, in pixels. Zero while
    // observed; grows linearly with prediction time (faster with no velocity
    // estimate), frozen once the prediction cap is reached.
    double prediction_uncertainty_px() const noexcept { return prediction_uncertainty_px_; }
    std::int64_t last_observation_ns() const noexcept { return last_observation_ns_; }
    std::int64_t creation_timestamp_ns() const noexcept { return creation_ns_; }

    // Milliseconds since the last observation at `now_ns` (>= 0). Feeds the
    // ADR-007 clause 5/6 age checks via the snapshot builder.
    double age_since_last_observation_ms(std::int64_t now_ns) const noexcept;

    // Lost and Retired tracks no longer own a per-type slot and must not
    // receive associations (plan R3 — fixes the fast-moving-ball lockout).
    bool accepts_observations() const noexcept {
        return state_ != TrackState::Lost && state_ != TrackState::Retired;
    }

private:
    std::uint32_t id_ = 0;
    ObjectType type_ = ObjectType::Ball;
    TrackState state_ = TrackState::Provisional;
    int observation_count_ = 0;
    int missed_cycles_ = 0;  // consecutive; Provisional deletion counter (plan R3)
    cv::Point2f position_px_{0.0F, 0.0F};       // predicted while coasting (TRK-016)
    cv::Point2f last_obs_position_px_{0.0F, 0.0F};  // observation-backed anchor
    cv::Point2f velocity_px_per_s_{0.0F, 0.0F};
    bool has_velocity_ = false;
    double prediction_uncertainty_px_ = 0.0;
    double radius_px_ = 0.0;
    std::int64_t last_observation_ns_ = 0;
    std::int64_t creation_ns_ = 0;

    // Config timeouts, converted to ns at construction.
    int confirm_threshold_ = 0;
    std::int64_t predict_timeout_ns_ = 0;
    std::int64_t occlude_timeout_ns_ = 0;
    std::int64_t retire_timeout_ns_ = 0;
    std::int64_t max_predict_duration_ns_ = 0;

    void predict(std::int64_t now_ns);  // TRK-016, called from tick() while coasting
};

}  // namespace tracking
