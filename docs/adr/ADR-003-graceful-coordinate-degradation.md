# ADR-003: Graceful Coordinate Degradation

## Status
Accepted (2026-05-08, originally documented in v0.2 design). v0.3 ships only the `Plane2D_World` (FloorPlane2D) path; full degradation hierarchy is implemented in Phase 3+.

## Context
A tracking system that fails entirely the moment one calibration assumption is violated is brittle. Real-world conditions — a bumped camera, a temporary calibration marker occlusion, a single-camera fallback when a sibling camera dies — should degrade output quality, not eliminate it. A consumer that can act on degraded coordinates (with elevated uncertainty) is more useful than one that receives nothing.

Forces:
- Calibration health varies continuously, not as a binary good/bad.
- The drone consumer can hover when uncertainty is high, but only if it is told the truth about uncertainty.
- The viewer and replay tools should continue to display objects even when world coordinates are unavailable.

## Decision
The tracking core will support three coordinate spaces, each successively less constrained, and will publish observations in the highest-quality space available given current calibration health:

1. **`Full3D_World`** — triangulated 3D position from two or more cameras with valid extrinsics. Phase 3+.
2. **`Plane2D_World`** — single-camera 2D position projected onto a known plane (the floor, per ADR-006), with object-class Z compensation per ADR-010. **This is the only space implemented in v0.3.**
3. **`ImagePixels`** — raw pixel coordinates, no calibration applied. Always available as a fallback.

Every published `position` field carries an explicit `coordinate_space` enum value, so downstream consumers can react to the space without re-deriving it.

When calibration health degrades, the core silently downgrades the coordinate space rather than dropping the observation. The published `uncertainty_m` reflects the degraded space (typically larger).

## Consequences

**Positive:**
- The viewer and replay logger remain useful even when calibration is invalid.
- The drone consumer (via the MAVLink adapter) sees `coordinate_space` change and can react (typically by withholding `VISION_POSITION_ESTIMATE` until the space recovers).
- Adding a second camera in Phase 3 is purely additive: existing single-camera consumers continue to work.

**Negative:**
- Three coordinate spaces is more code than one. Mitigated by implementing only `Plane2D_World` and `ImagePixels` in v0.3 — `Full3D_World` is a Phase 3 deliverable.
- Consumers must explicitly handle space changes; consumers that ignore `coordinate_space` will silently misinterpret data when degradation occurs.

**Risks:**
- Silent space degradation may surprise consumers that don't read `coordinate_space`. Mitigated by including the field in every `position` block (no implicit defaults) and by failing schema validation on the consumer side if the field is missing.

## Alternatives Considered
- **Binary calibrated/uncalibrated state.** Rejected: a calibrated/uncalibrated boolean does not capture "calibration is degraded but better than nothing," which is the most common real-world state.
- **Always publish in the highest space (Full3D), substituting estimated values when measurements are unavailable.** Rejected: this conflates measurement and estimation in a way that hides genuine uncertainty from the consumer.

## Related ADRs
- ADR-006 (FloorPlane2D World Frame) — defines the Plane2D_World coordinate frame for v0.3.
- ADR-010 (Object-Class Z Compensation) — applied when computing Plane2D_World positions.
- ADR-004 (Hybrid Calibration Lifecycle) — provides the calibration health signal that drives degradation.
