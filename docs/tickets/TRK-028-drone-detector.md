---
id: TRK-028
status: backlog
subsystem: tracking-core
tier: design
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-027"
spec: null
plan: null
blockers: []
---

## Context

Phase 3 adds the drone as a tracked object. The tracking core observes the drone in the camera frame and publishes its floor-plane position. The drone detector is a new detector module alongside the laser and ball detectors. It does NOT control the drone — it only observes it. The drone's position in the floor frame feeds into the MAVLink adapter (MAV-001) which provides external position estimates to the drone's EKF.

Detection approach constraints:
- No GPU on Pi 5 → no neural network inference at frame rate.
- Drone is a known object of known approximate size (span ~25–40cm depending on build).
- LED markers on the drone (user-controlled, e.g., IR LEDs or coloured LEDs) are the likely primary detection signal.
- Alternative: ArUco marker on top of drone visible to ceiling-mounted camera.
- Z compensation per ADR-010: drone hovers at variable height, requiring dynamic Z (altitude from barometer or estimated from apparent size).

## Acceptance

- A `DroneDetector` module that detects the drone in each frame and produces `DroneObservation`.
- Detection signal: LED markers or ArUco marker (design decision required).
- `DroneObservation` includes image-plane position, apparent size, and confidence.
- Z compensation: drone height is NOT zero. Either estimated from apparent size or received from an external altitude source.
- Track lifecycle: same state machine as other objects (Provisional → Confirmed → ...).
- Performance: detector must fit within the remaining frame budget after laser + ball detection.
- Graceful absence: when the drone is not in frame, no track is created (not a failure state).

## Plan

Tier `design` — requires decision on detection signal (LEDs vs markers vs blob) and Z estimation strategy.

### Sub-tickets (to be created when Phase 3 begins)

| Child | Title | Scope |
|-------|-------|-------|
| TRK-028a | Drone detection strategy decision | LED vs ArUco vs blob, trade-off analysis |
| TRK-028b | Drone detector implementation | Detection module following chosen strategy |
| TRK-028c | Drone Z estimation | Height from apparent size, baro input, or fixed assumption |
| TRK-028d | Integration + drone track lifecycle | Wire into pipeline, validate with recorded footage |

### Open design questions

1. **Detection signal:** IR LEDs (consistent with NoIR camera), visible LEDs, or ArUco marker?
2. **Z estimation:** apparent-size method (noisy), external barometer via ZMQ subscription, or assume fixed hover height?
3. **Multi-drone:** v0.3+ assumes one drone. Should the architecture support multiple? (Probably not until much later.)

## Log

- 2026-05-31: created. Status: backlog. Phase 3 gate. Depends on v0.3 pipeline + multi-camera (TRK-027) for adequate coverage.
