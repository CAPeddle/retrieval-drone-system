// TRK-014: track lifecycle state machine implementation. Transition semantics
// per the v0.3 slice plan (R1, R3, R4, R11); cited thresholds come from the
// `track` config section.

#include "track.hpp"

#include <cmath>

namespace tracking {

namespace {

// Config carries milliseconds; timers run on the capture clock in ns.
std::int64_t ms_to_ns(double ms) noexcept {
    return static_cast<std::int64_t>(std::llround(ms * 1.0e6));
}

// A Provisional track that misses this many consecutive cycles is deleted
// without entering the Predicted/Occluded decay chain (plan R3): a one-frame
// false detection must not hold a per-type slot against the real object.
constexpr int kProvisionalMissLimit = 2;

// TRK-016 prediction-uncertainty growth rates (px per predicted ms). With a
// velocity estimate the extrapolation is informed; without one it is a pure
// hold, so uncertainty grows faster. Both provisional pending replay-footage
// validation (same provenance rule as the `track` config defaults).
constexpr double kPredictUncertaintyPxPerMs = 0.5;
constexpr double kNoVelocityUncertaintyPxPerMs = 1.0;

}  // namespace

Track::Track(std::uint32_t id, const Observation& first, const TrackConfig& config)
    : id_(id),
      type_(first.type),
      state_(TrackState::Provisional),
      observation_count_(1),
      position_px_(first.centroid_px),
      last_obs_position_px_(first.centroid_px),
      radius_px_(first.radius_px),
      last_observation_ns_(first.capture_timestamp_ns),
      creation_ns_(first.capture_timestamp_ns),
      confirm_threshold_(config.confirm_threshold),
      predict_timeout_ns_(ms_to_ns(config.predict_timeout_ms)),
      occlude_timeout_ns_(ms_to_ns(config.occlude_timeout_ms)),
      retire_timeout_ns_(ms_to_ns(config.retire_timeout_ms)),
      max_predict_duration_ns_(ms_to_ns(config.max_predict_duration_ms)) {}

void Track::observe(const Observation& observation) {
    if (!accepts_observations()) {
        return;  // Lost/Retired: defence in depth, see header.
    }
    // TRK-016: velocity from the last two observation-backed positions. A
    // non-positive dt (duplicate/out-of-order capture timestamp) retains the
    // previous estimate rather than dividing toward NaN/inf (plan R4 dt guard).
    const std::int64_t dt_ns = observation.capture_timestamp_ns - last_observation_ns_;
    if (dt_ns > 0) {
        const float dt_s = static_cast<float>(dt_ns) / 1.0e9F;
        velocity_px_per_s_ = (observation.centroid_px - last_obs_position_px_) / dt_s;
        has_velocity_ = observation_count_ >= 1;  // needs a prior observation
    }
    ++observation_count_;
    missed_cycles_ = 0;
    position_px_ = observation.centroid_px;
    last_obs_position_px_ = observation.centroid_px;
    prediction_uncertainty_px_ = 0.0;
    radius_px_ = observation.radius_px;
    last_observation_ns_ = observation.capture_timestamp_ns;
}

void Track::predict(std::int64_t now_ns) {
    // Extrapolate from the last OBSERVED position, capped at
    // max_predict_duration_ms — beyond the cap the position freezes and the
    // state machine owns further degradation (TRK-016 acceptance).
    const std::int64_t age_ns = now_ns - last_observation_ns_;
    const std::int64_t predict_ns =
        age_ns < max_predict_duration_ns_ ? age_ns : max_predict_duration_ns_;
    if (predict_ns <= 0) {
        return;
    }
    if (has_velocity_) {
        const float dt_s = static_cast<float>(predict_ns) / 1.0e9F;
        position_px_ = last_obs_position_px_ + velocity_px_per_s_ * dt_s;
    }
    const double predict_ms = static_cast<double>(predict_ns) / 1.0e6;
    const double rate =
        has_velocity_ ? kPredictUncertaintyPxPerMs : kNoVelocityUncertaintyPxPerMs;
    prediction_uncertainty_px_ = rate * predict_ms;
}

void Track::tick(bool observed, std::int64_t now_ns) {
    if (state_ == TrackState::Retired) {
        return;
    }

    if (observed) {
        switch (state_) {
            case TrackState::Provisional:
                if (observation_count_ >= confirm_threshold_) {
                    state_ = TrackState::Confirmed;
                }
                break;
            case TrackState::Predicted:
            case TrackState::Occluded:
                // One associated observation restores trust (plan R3).
                state_ = TrackState::Confirmed;
                break;
            case TrackState::Confirmed:
            case TrackState::Lost:
            case TrackState::Retired:
                break;  // Confirmed stays; Lost never sees observed=true.
        }
        return;
    }

    // Unobserved cycle. Age against the capture clock; ties decay (>=, R11).
    const std::int64_t age_ns = now_ns - last_observation_ns_;

    if (state_ == TrackState::Provisional) {
        ++missed_cycles_;
        if (missed_cycles_ >= kProvisionalMissLimit) {
            state_ = TrackState::Retired;
        }
        return;
    }

    // Decay cascade — each step visits the next state in order; a single tick
    // after a long gap may traverse several (plan R4), never skipping one.
    if (state_ == TrackState::Confirmed) {
        state_ = TrackState::Predicted;
    }
    if (state_ == TrackState::Predicted && age_ns >= predict_timeout_ns_) {
        state_ = TrackState::Occluded;
    }
    if (state_ == TrackState::Occluded && age_ns >= occlude_timeout_ns_) {
        state_ = TrackState::Lost;
    }
    if (state_ == TrackState::Lost && age_ns >= retire_timeout_ns_) {
        state_ = TrackState::Retired;
    }

    // TRK-016: coasting states extrapolate; Lost/Retired positions freeze.
    if (state_ == TrackState::Predicted || state_ == TrackState::Occluded) {
        predict(now_ns);
    }
}

double Track::age_since_last_observation_ms(std::int64_t now_ns) const noexcept {
    const std::int64_t age_ns = now_ns - last_observation_ns_;
    return age_ns > 0 ? static_cast<double>(age_ns) / 1.0e6 : 0.0;
}

}  // namespace tracking
