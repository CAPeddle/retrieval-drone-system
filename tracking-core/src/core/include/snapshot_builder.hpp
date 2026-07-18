#pragma once

// TRK-020 / plan U8: assembles the per-frame TrackingSnapshot from the live
// track set. Owns the two pieces of cross-frame state the snapshot needs that
// tracks do not carry: per-type observation-backed floor-position history for
// speed_m_per_s (predicted positions never feed speed), and the
// INITIALISING -> RUNNING transition (first quality-admitted frame).

#include <cstdint>
#include <vector>

#include "config.hpp"
#include "coordinate_mapper.hpp"
#include "track.hpp"
#include "tracking_snapshot.hpp"

namespace tracking {

class SnapshotBuilder {
public:
    SnapshotBuilder(const CoordinateMapper& mapper, const CoordinateConfig& coordinate);

    // First quality-admitted frame flips tracker_state to RUNNING (plan U8).
    void mark_frame_admitted() noexcept { running_ = true; }
    void note_frame_processed() noexcept { ++frames_processed_; }
    void note_frame_rejected() noexcept { ++frames_rejected_; }
    bool running() const noexcept { return running_; }

    // Builds the snapshot for this cycle. Hot path: no allocation, no
    // exceptions; every float finite-checked (plan R8) — a non-finite value
    // invalidates its slot and increments health.sanitised_slots.
    TrackingSnapshot build(const std::vector<Track>& tracks,
                           std::int64_t frame_capture_ns, std::int64_t now_ns);

private:
    // Observation-backed speed state per object type (plan U8): updated only
    // when a NEW observation timestamp appears on the same track; dt <= 0 or a
    // track-id change resets rather than dividing toward NaN.
    struct SpeedState {
        std::uint32_t track_id = 0;
        std::int64_t last_obs_ns = 0;
        double floor_x_m = 0.0;
        double floor_y_m = 0.0;
        double speed_m_per_s = 0.0;
        bool has_sample = false;
        bool speed_valid = false;
    };

    void fill_slot(ObjectSlot& slot, SpeedState& speed, const Track& track,
                   std::int64_t now_ns, std::uint32_t& sanitised);

    const CoordinateMapper* mapper_;
    double pixel_uncertainty_stddev_px_ = 0.0;
    SpeedState laser_speed_;
    SpeedState ball_speed_;
    bool running_ = false;
    std::uint64_t frames_processed_ = 0;
    std::uint64_t frames_rejected_ = 0;
};

}  // namespace tracking
