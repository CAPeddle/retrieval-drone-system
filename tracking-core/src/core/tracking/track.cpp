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

}  // namespace

Track::Track(std::uint32_t id, const Observation& first, const TrackConfig& config)
    : id_(id),
      type_(first.type),
      state_(TrackState::Provisional),
      observation_count_(1),
      position_px_(first.centroid_px),
      radius_px_(first.radius_px),
      last_observation_ns_(first.capture_timestamp_ns),
      creation_ns_(first.capture_timestamp_ns),
      confirm_threshold_(config.confirm_threshold),
      predict_timeout_ns_(ms_to_ns(config.predict_timeout_ms)),
      occlude_timeout_ns_(ms_to_ns(config.occlude_timeout_ms)),
      retire_timeout_ns_(ms_to_ns(config.retire_timeout_ms)) {}

void Track::observe(const Observation& observation) {
    if (!accepts_observations()) {
        return;  // Lost/Retired: defence in depth, see header.
    }
    ++observation_count_;
    missed_cycles_ = 0;
    position_px_ = observation.centroid_px;
    radius_px_ = observation.radius_px;
    last_observation_ns_ = observation.capture_timestamp_ns;
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
}

double Track::age_since_last_observation_ms(std::int64_t now_ns) const noexcept {
    const std::int64_t age_ns = now_ns - last_observation_ns_;
    return age_ns > 0 ? static_cast<double>(age_ns) / 1.0e6 : 0.0;
}

}  // namespace tracking
