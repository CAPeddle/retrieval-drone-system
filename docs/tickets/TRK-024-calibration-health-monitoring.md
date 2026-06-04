---
id: TRK-024
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-011"
  - "TRK-017"
  - "TRK-023"
spec: null
plan: null
blockers: []
---

## Context

ADR-004 Phase 2: the tracking core continuously monitors a static AprilTag/Charuco marker in the camera's field of view. It compares the marker's current reprojected floor coordinate against its known calibration-time coordinate. Deviation drives `calibration_status` between VALID, DEGRADED, and INVALID. This detects camera bumps, focus drift, or floor-frame shifts without operator intervention.

Key thresholds per ADR-004:
- `CAL_HEALTH_WARN_M` (default 0.005 m) → DEGRADED
- `CAL_HEALTH_FAIL_M` (default 0.020 m) → INVALID
- Absence < 1s → occlusion (no status change); consistent shift > 5s → real degradation.

## Acceptance

- A `CalibrationHealthMonitor` class that consumes `MarkerObservation`s from TRK-011 and known floor positions from TRK-017.
- Computes deviation: `||observed_floor_pos - expected_floor_pos||` for the monitored marker.
- State transitions:
  - deviation < WARN → VALID.
  - WARN ≤ deviation < FAIL → DEGRADED.
  - deviation ≥ FAIL consistently for > 5s → INVALID.
- Occlusion handling: marker not detected for < 1s → no status change; > 1s → DEGRADED (might have been moved while occluded).
- Status reported to `HealthAggregator` (TRK-023).
- When INVALID: core stops publishing `Plane2D_World` coordinates (falls back to `ImagePixels` per ADR-003).
- Config fields: `calibration.health_warn_m` (default 0.005), `calibration.health_fail_m` (default 0.020), `calibration.occlusion_grace_s` (default 1.0), `calibration.shift_confirm_s` (default 5.0).
- Unit tests: stable marker → VALID; small shift → DEGRADED; large sustained shift → INVALID; brief occlusion → no change; long occlusion → DEGRADED.

## Plan

U1. Create `src/core/calibration/health_monitor.hpp` with `CalibrationHealthMonitor` class.
U2. Implement per-frame update: receive marker observation, project to floor, compute deviation from known position.
U3. Implement state machine: VALID ↔ DEGRADED ↔ INVALID with temporal guards (shift_confirm_s for sustained deviation).
U4. Implement occlusion detection: track consecutive frames without marker; apply grace period.
U5. Report status changes to `HealthAggregator`.
U6. When INVALID, set flag that CoordinateMapper reads to downgrade output to `ImagePixels`.
U7. Add config fields for thresholds and temporal guards.
U8. Unit tests: all state transitions, temporal guards, occlusion scenarios.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-011 (marker detection), TRK-017 (known floor positions), TRK-018 (coordinate mapping to compare), TRK-023 (health reporting).
