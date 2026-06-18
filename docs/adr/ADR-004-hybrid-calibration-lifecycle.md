# ADR-004: Hybrid Calibration Lifecycle

## Status
Accepted (2026-05-08, originally documented in v0.2 design). v0.3 implements Phases 1 and 2; Phase 3 (active dynamic refinement) is captured separately as ADR-009 with status Proposed.

## Context
Calibration is not a one-time setup activity; it is a continuous concern. A camera that was perfectly calibrated at install can drift due to thermal expansion, mechanical bumps, focus changes, or table movement. A single calibration health signal is insufficient because different problems (intrinsics drift vs. extrinsics drift vs. floor-frame shift) require different responses.

Forces:
- Manual calibration is laborious and should not be required at every system start.
- A static reference (an AprilTag in the camera's field of view, fixed to the floor or a known object) gives us a continuous, low-cost health signal.
- Automated dynamic refinement (e.g., using the laser as a calibration probe) is theoretically attractive but has unproven observability and depends on actuator accuracy that has not yet been characterised.

## Decision
Calibration is a three-phase lifecycle. Each phase is independently testable, and later phases extend earlier ones rather than replacing them.

**Phase 1 — Manual Initialisation (v0.3, accepted)**
- Per-camera intrinsic calibration using OpenCV `cv2.calibrateCamera()` with a Charuco board, performed offline.
- Per-camera extrinsic calibration to the floor frame using `cv2.findHomography()` with at least 4 known floor anchor points (Charuco corners placed on the floor at install time).
- Calibration data is persisted as versioned JSON (`{intrinsics_version, extrinsics_version, calibration_timestamp}`) and validated at core startup.
- Per-unit intrinsic files are required. Shared profiles (e.g., one file for the OV5647 NoIR sensor family) are permitted only as a bootstrap aid for development; production deployment requires per-unit calibration.

**Phase 2 — Static Marker Health Monitoring (v0.3, accepted)**
- A static AprilTag (or Charuco subset) is fixed within each camera's field of view at calibration time, in a position unlikely to be occluded during normal operation.
- The tracking core continuously detects this marker as a CalibrationMarker observation type.
- The marker's reprojected floor coordinate is compared against its known calibration-time coordinate every N frames.
- If the deviation exceeds `CAL_HEALTH_WARN_M` (default 0.005 m), `calibration_status` becomes `DEGRADED`.
- If it exceeds `CAL_HEALTH_FAIL_M` (default 0.020 m), `calibration_status` becomes `INVALID`. The camera continues publishing `ImagePixels` observations but ceases publishing `Plane2D_World` observations.

**Phase 3 — Active Dynamic Refinement (Phase 5+, proposed in ADR-009)**
- Use the laser as an actively-controlled calibration probe to refine the homography under operating conditions.
- Captured as ADR-009 with status `Proposed`. Observability of the underlying optimisation problem must be experimentally validated before this phase is implemented.

## Consequences

**Positive:**
- Calibration drift is detectable continuously, not just at scheduled re-calibration windows.
- The system degrades gracefully (per ADR-003) when calibration is compromised, rather than crashing.
- Intrinsic and extrinsic calibration are separable concerns: a camera can have valid intrinsics but failed extrinsics (e.g., camera was bumped) and the system handles this case correctly.

**Negative:**
- Each camera install requires placing a Charuco board on the floor and a static AprilTag in the field of view. Ten to fifteen minutes of human time per camera.
- The static marker consumes a portion of the field of view that could otherwise be used for tracking.

**Risks:**
- The static marker can itself be occluded (a child sits on it). The system must distinguish "marker occluded" (briefly absent) from "marker shifted" (consistently visible at a wrong location). Mitigated by treating absence < 1 second as an occlusion (no calibration status change), and a consistent shift over > 5 seconds as a real degradation.
- Per-unit intrinsics introduce a manufacturing/install step that may be skipped under time pressure. Mitigated by enforcing intrinsic-file presence at startup and refusing to enter `RUNNING` state without it.

## Alternatives Considered
- **One-time calibration only.** Rejected: any physical disturbance during operation goes undetected and silently corrupts world coordinates.
- **Fully online calibration (no manual phase).** Rejected for v0.3: insufficient observability, no characterised actuator accuracy, no benchmark to compare against. Captured as a future direction in ADR-009.
- **Periodic forced re-calibration on a schedule.** Rejected: disruptive to operation, doesn't react to events between scheduled windows.

## Related ADRs
- ADR-003 (Graceful Coordinate Degradation) — consumes the calibration health signal.
- ADR-006 (FloorPlane2D World Frame) — defines what is being calibrated.
- ADR-009 (Active Calibration Refinement) — proposes Phase 3 with explicit observability validation.
