// TRK-020 / plan U8: snapshot assembly implementation.

#include "snapshot_builder.hpp"

#include <cmath>

namespace tracking {

SnapshotBuilder::SnapshotBuilder(const CoordinateMapper& mapper,
                                 const CoordinateConfig& coordinate)
    : mapper_(&mapper),
      pixel_uncertainty_stddev_px_(coordinate.pixel_uncertainty_stddev_px) {}

void SnapshotBuilder::fill_slot(ObjectSlot& slot, SpeedState& speed, const Track& track,
                                std::int64_t now_ns, std::uint32_t& sanitised) {
    slot.object_type = static_cast<std::uint8_t>(track.type());
    slot.track_state = static_cast<std::uint8_t>(track.state());
    slot.track_id = track.id();
    slot.pixel_x_px = track.position_px().x;
    slot.pixel_y_px = track.position_px().y;
    slot.age_ms = track.age_since_last_observation_ms(now_ns);

    const FloorPosition pos = mapper_->project_to_floor(track.position_px(), track.type());
    if (!pos.valid) {
        slot.valid = false;  // degenerate mapping: predicate fails clause 3/4
        return;
    }
    // Combine mapping and prediction uncertainty: the mapper's value is for
    // the configured pixel stddev alone; widen it for the track's coasting
    // uncertainty (plan U8): sigma_total = sigma_J * sqrt(px^2 + pred^2).
    const double px = pixel_uncertainty_stddev_px_;
    const double pred = track.prediction_uncertainty_px();
    const double uncertainty =
        pos.uncertainty_m / px * std::sqrt(px * px + pred * pred);

    if (!std::isfinite(pos.x_m) || !std::isfinite(pos.y_m) || !std::isfinite(uncertainty) ||
        !std::isfinite(slot.age_ms) || !std::isfinite(static_cast<double>(slot.pixel_x_px)) ||
        !std::isfinite(static_cast<double>(slot.pixel_y_px))) {
        ++sanitised;
        slot.valid = false;  // no NaN ever leaves the snapshot (plan R8)
        return;
    }

    slot.floor_x_m = pos.x_m;
    slot.floor_y_m = pos.y_m;
    slot.floor_z_m = pos.z_m;
    slot.uncertainty_m = uncertainty;
    slot.valid = true;

    // Observation-backed speed (plan U8). A new track id resets history; a new
    // observation timestamp on the same track advances it.
    if (speed.track_id != track.id()) {
        speed = SpeedState{};
        speed.track_id = track.id();
    }
    if (!speed.has_sample) {
        speed.last_obs_ns = track.last_observation_ns();
        speed.floor_x_m = pos.x_m;
        speed.floor_y_m = pos.y_m;
        speed.has_sample = true;
    } else if (track.last_observation_ns() != speed.last_obs_ns) {
        const std::int64_t dt_ns = track.last_observation_ns() - speed.last_obs_ns;
        // Only observation-backed positions feed speed: the slot position at
        // an observation cycle IS the observed position (prediction only runs
        // while unobserved).
        if (dt_ns > 0) {
            const double dt_s = static_cast<double>(dt_ns) / 1.0e9;
            const double dist = std::hypot(pos.x_m - speed.floor_x_m,
                                           pos.y_m - speed.floor_y_m);
            const double candidate = dist / dt_s;
            if (std::isfinite(candidate)) {
                speed.speed_m_per_s = candidate;
                speed.speed_valid = true;
            }
        }
        // dt <= 0 (out-of-order timestamps): retain the previous estimate.
        speed.last_obs_ns = track.last_observation_ns();
        speed.floor_x_m = pos.x_m;
        speed.floor_y_m = pos.y_m;
    }
    slot.speed_m_per_s = speed.speed_valid ? speed.speed_m_per_s : 0.0;
    slot.speed_valid = speed.speed_valid;
}

TrackingSnapshot SnapshotBuilder::build(const std::vector<Track>& tracks,
                                        std::int64_t frame_capture_ns,
                                        std::int64_t now_ns) {
    TrackingSnapshot snapshot;
    snapshot.frame_capture_timestamp_ns = frame_capture_ns;
    snapshot.snapshot_timestamp_ns = now_ns;
    snapshot.health.tracker_state =
        running_ ? TrackerState::Running : TrackerState::Initialising;
    snapshot.health.calibration_status = CalibrationStatus::Valid;  // KTD-7
    snapshot.health.frames_processed = frames_processed_;
    snapshot.health.frames_rejected = frames_rejected_;

    std::uint32_t sanitised = 0;
    for (const Track& track : tracks) {
        if (track.type() == ObjectType::Laser && !snapshot.laser.valid) {
            fill_slot(snapshot.laser, laser_speed_, track, now_ns, sanitised);
        } else if (track.type() == ObjectType::Ball && !snapshot.ball.valid) {
            fill_slot(snapshot.ball, ball_speed_, track, now_ns, sanitised);
        }
    }
    snapshot.health.sanitised_slots = sanitised;
    return snapshot;
}

}  // namespace tracking
