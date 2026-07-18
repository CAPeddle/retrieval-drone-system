#pragma once

// TRK-020 / plan KTD-3: the per-frame, fixed-size, trivially-copyable summary
// of everything the core believes at one instant. The ADR-007 predicate is a
// pure function of one snapshot; the TRK-021 publisher serialises it on the
// export thread. No vectors, no strings — reason names and JSON structure
// materialise only at serialisation.

#include <cstdint>
#include <type_traits>

namespace tracking {

// System-level pipeline state (minimal v0.3 set; DEGRADED arrives with the
// thermal work in TRK-023 — do not conflate with FrameQuality::DEGRADED).
enum class TrackerState : std::uint8_t { Initialising, Running };

// Constant Valid in v0.3 after the fail-fast startup load (plan KTD-7);
// Degraded/Invalid arrive with TRK-024 calibration health monitoring.
enum class CalibrationStatus : std::uint8_t { Valid };

// ADR-007 unsafe reasons as bitmask bits (plan R13). Bit 7 is reserved for
// the v0.4 clause 9 uncertainty margin.
enum UnsafeReason : std::uint16_t {
    kTrackerNotRunning = 1U << 0,     // clause 1
    kCalibrationInvalid = 1U << 1,    // clause 2
    kLaserNotConfirmed = 1U << 2,     // clause 3
    kBallNotConfirmed = 1U << 3,      // clause 4
    kAgeExceedsThreshold = 1U << 4,   // clauses 5/6
    kLaserInTransit = 1U << 5,        // clause 7
    kLaserBallMisaligned = 1U << 6,   // clause 8
};

// One tracked object (laser or ball). `valid` false means "no live track of
// this type this frame" or "floor projection degenerate" — the predicate then
// fails the object's Confirmed clause (fail on ambiguity, ADR-007).
struct ObjectSlot {
    bool valid = false;
    std::uint8_t object_type = 0;      // tracking::ObjectType value
    std::uint8_t track_state = 0;      // tracking::TrackState value
    std::uint32_t track_id = 0;
    float pixel_x_px = 0.0F;
    float pixel_y_px = 0.0F;
    double floor_x_m = 0.0;
    double floor_y_m = 0.0;
    double floor_z_m = 0.0;
    double uncertainty_m = 0.0;        // mapping + prediction, 1-sigma
    double age_ms = 0.0;               // since last observation, capture clock
    double speed_m_per_s = 0.0;        // observation-backed only (plan U8)
    bool speed_valid = false;          // needs two observations (plan R12)
};

struct SafetyResult {
    bool safe = false;
    std::uint16_t unsafe_reasons = 0;          // UnsafeReason bitmask
    std::uint32_t hysteresis_remaining_ms = 0; // until flip-to-true permitted
    double laser_ball_distance_m = 0.0;        // clause-8 distance (viewer)
    bool distance_valid = false;               // both slots valid and mapped
};

struct SystemHealthSnapshot {
    TrackerState tracker_state = TrackerState::Initialising;
    CalibrationStatus calibration_status = CalibrationStatus::Valid;
    std::uint64_t frames_processed = 0;
    std::uint64_t frames_rejected = 0;
    std::uint32_t sanitised_slots = 0;  // non-finite values invalidated (plan R8)
};

struct TrackingSnapshot {
    std::int64_t frame_capture_timestamp_ns = 0;
    std::int64_t snapshot_timestamp_ns = 0;
    ObjectSlot laser;
    ObjectSlot ball;
    SafetyResult safety;
    SystemHealthSnapshot health;
};

// Hot-path contract: copied by value through the export SPSC ring (plan KTD-3).
static_assert(std::is_trivially_copyable_v<TrackingSnapshot>,
              "TrackingSnapshot crosses the export ring by memcpy");
static_assert(sizeof(TrackingSnapshot) <= 512,
              "TrackingSnapshot must stay ring-slot sized");

}  // namespace tracking
