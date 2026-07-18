#pragma once

// TRK-021: serialises TrackingSnapshot to the v0.3 JSON schema and publishes
// over ZMQ PUB per ADR-002. Runs on the export thread, never the hot path.
// zmq types stay out of this header (pimpl) so cppzmq remains a PRIVATE
// dependency of tracking_core_lib.
//
// ── v0.3 wire schema (schema_version 1) ─────────────────────────────────────
// The single C++-side source of truth, mirrored by the pydantic models in
// src/viewer/schema.py. SINGLE-PART message, no topic envelope: libzmq's
// ZMQ_CONFLATE does not support multipart, so a topic frame would silently
// break the freshest-wins contract; consumers subscribe with the empty
// filter and set RCVHWM=1, CONFLATE=1.
//
// {
//   "schema_version": 1,
//   "message_id": <uint64, monotonic from 0; GAPS ARE NORMAL under
//                  conflation and are not data loss>,
//   "session_id": <uint64, wall-clock epoch ms at core startup; consumers
//                  reset per-session state (trails, timelines) on change>,
//   "publish_timestamp_ms": <uint64, wall-clock epoch ms>,
//   "frame_capture_timestamp_ms": <uint64, wall-clock epoch ms, derived from
//                  the capture clock via a per-message steady->wall offset>,
//   "system_health": {
//     "tracker_state": "INITIALISING" | "RUNNING",
//     "calibration_status": "VALID",
//     "ball_radius_m": <double>,          // ADR-007 sanity-check mitigation
//     "cpu_temp_c": <double, -1.0 when unreadable>,
//     "frames_processed_count": <uint64>,
//     "frames_rejected_count": <uint64>
//     // frame_drop_rate + thermal_throttled arrive with TRK-023
//   },
//   "thresholds": {                       // static per session, from config
//     "age_max_ms", "laser_settled_speed_m_per_s",
//     "alignment_tolerance_m", "min_unsafe_dwell_ms"
//   },
//   "objects": [                          // 0..2 entries; invalid slots omitted
//     {
//       "object_type": "laser" | "ball",
//       "track_id": <uint32>, "track_state": "Provisional".."Retired",
//       "position": { "coordinate_space": "Plane2D_World",
//                     "x_m", "y_m", "z_m", "uncertainty_m" },
//       "pixel": { "x_px", "y_px" },
//       "age_ms": <double>,
//       "speed_m_per_s": <double>, "speed_valid": <bool>,
//       "safe_for_control": <bool>,       // global predicate (ADR-002)
//       "unsafe_reasons": [ <string>... ]
//     }
//   ],
//   "safety": {
//     "safe": <bool>, "unsafe_reasons": [ <string>... ],
//     "hysteresis_remaining_ms": <uint32>,
//     "laser_ball_distance_m": <double>,  // present only when distance_valid
//     "distance_valid": <bool>
//   }
// }
//
// Consumer staleness rule (plan R17, MANDATORY for any controlling consumer):
// a message older than 100 ms by LOCAL receive time is stale — do not act on
// its safe_for_control. With the heartbeat deferred (TRK-022) this rule is
// the only staleness defence. Never compare producer wall-clock timestamps
// against another host's clock (the Pi has no NTP).

#include <cstdint>
#include <memory>

#include "config.hpp"
#include "tracking_snapshot.hpp"

namespace tracking {

class ZmqPublisher {
public:
    // Creates the PUB socket (SNDHWM=1, CONFLATE=1, LINGER=0) and binds
    // zmq.bind_address. Startup context: may throw.
    ZmqPublisher(const ZmqConfig& zmq_config, const SafeForControlConfig& safety_config,
                 double ball_radius_m);
    ~ZmqPublisher();
    ZmqPublisher(const ZmqPublisher&) = delete;
    ZmqPublisher& operator=(const ZmqPublisher&) = delete;

    // Serialises and sends one snapshot (export thread only). `cpu_temp_c` is
    // the export thread's 1 Hz sysfs reading (-1.0 when unreadable).
    void publish(const TrackingSnapshot& snapshot, double cpu_temp_c);

    std::uint64_t messages_published() const noexcept;
    std::uint64_t session_id() const noexcept;

    // Socket options as actually applied (getsockopt), for tests.
    struct SocketOptions {
        int sndhwm = 0;
        int conflate = 0;
        int linger = -1;
    };
    SocketOptions socket_options() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tracking
