# ADR-010: Object-Class Z Compensation in Coordinate Mapping

## Status
Accepted (2026-05-08)

## Context
The FloorPlane2D world frame (ADR-006) defines `Z = 0` as the floor surface. Naively projecting an image-plane detection onto `Z = 0` via the per-camera homography is correct only for objects whose centroid lies exactly on the floor — i.e., the laser dot. A foam ball resting on the floor has its centroid at `Z = ball.radius_m`, not `Z = 0`. Projecting a ball's centroid pixel through `Z = 0` introduces a position bias along the camera ray.

Magnitude of the bias depends on camera tilt. For the v0.3 deployment ("tabletop-mounted, pointed downwards at floor"), the camera will typically be at roughly 70–100 cm height looking forward at 30–60° tilt. Under representative geometry — camera at 1 m height, 45° tilt, ball at 2 m horizontal distance, ball radius 3 cm — the bias is approximately 3 cm in floor coordinates. This is **larger** than the `safe_for_control` alignment tolerance of 2 cm (ADR-007), and would produce systematic false negatives on the safety predicate.

This bias is correctable analytically given the camera's intrinsic parameters and the object's expected height.

## Decision
The CoordinateMapper applies object-class-aware Z compensation when projecting image observations onto the FloorPlane2D world frame.

### Algorithm

For each `ImageObservation` with image-plane centroid `(u, v)` and `object_type T`:

1. Look up `expected_height(T)`:
   - `LaserPoint`: `Z = 0`
   - `Ball`: `Z = ball.radius_m`
   - `Drone`: `Z = drone.observed_altitude_m` (Phase 3+; deferred — drone is not tracked in v0.3)
   - `CalibrationMarker`: `Z = 0`

2. Compute the camera ray through `(u, v)` using the camera's inverse intrinsics (undistort first, then back-project): the ray originates at the camera centre and points toward the world.

3. Compute the intersection of that ray with the plane `Z = expected_height(T)`.

4. Report the intersection point as `(x_m, y_m)` in the FloorPlane2D frame; report `z_m = expected_height(T)`.

### Required inputs

- Per-camera **intrinsic calibration** is required for every camera before the core enters `RUNNING` state. Without intrinsics, the back-projection step is undefined.
- **Per-unit intrinsic files are required for production deployment.** A shared intrinsic profile across multiple cameras of the same model (e.g., the OV5647 NoIR sensor) is permitted only as a **bootstrap aid for early development**. Production or accuracy-sensitive use must use per-unit calibration. The system will load per-unit intrinsics if available and fall back to the shared profile only if explicitly configured to do so, with a `INTRINSICS_SHARED_PROFILE` flag in `system_health` so consumers know.
- `ball.radius_m` is a static configuration parameter in v0.3 (per ADR-007), hot-reloadable from a UI in v0.4+.

### Uncertainty propagation

The pixel-detection uncertainty (a 1-pixel standard deviation on the image plane) propagates through the ray-plane intersection to a position uncertainty on the floor plane. The propagation Jacobian depends on camera tilt and distance to the object. The CoordinateMapper computes this uncertainty per observation and reports it as `uncertainty_m`. Far-from-camera observations have larger floor-plane uncertainty than near-camera observations, even if the pixel uncertainty is identical.

## Consequences

**Positive:**
- Eliminates a 3 cm systematic bias on ball positions, putting the residual error well within the 2 cm `safe_for_control` alignment tolerance.
- The `ALIGNMENT_TOLERANCE_M` parameter now means what it says — pure tolerance, not bias-plus-tolerance.
- Position uncertainty is no longer assumed-uniform across the image; far-edge detections correctly carry larger uncertainty.

**Negative:**
- Requires per-camera intrinsic calibration as a startup prerequisite. This is an additional install-time step.
- Requires `ball.radius_m` to be correct. A misconfigured radius produces a position bias in the *opposite* direction.
- The ray-plane intersection arithmetic is slightly more expensive than a simple homography multiplication. Cost is negligible (< 0.1 ms per observation on Pi 5).

**Risks:**
- **Ball not on floor (airborne).** A bouncing ball spends time at `Z > ball.radius_m`. Projecting through `Z = ball.radius_m` produces a position that is biased forward along the camera ray during airtime. **For v0.3 (laser tracking only, ball is not tracked), not a problem.** When ball tracking is added in v0.4, the BallDetector should mark observations as `airborne_suspected: true` when the apparent ball size in pixels deviates from the calibrated size at that floor position; the Tracker can then hold Z floating or downgrade `safe_for_control`. **Captured as a v0.4 design requirement.**
- **Misconfigured `ball.radius_m`.** A 5 cm error in radius causes a corresponding bias in floor coordinates. Mitigated by surfacing `ball.radius_m` in the published `system_health` block; the operator can sanity-check by comparing the published value against the physical ball.
- **Shared intrinsic profile used in production.** Documented in the ADR as a bootstrap-only accommodation; flagged in `system_health` so consumers know.

## Alternatives Considered
- **Widen `ALIGNMENT_TOLERANCE_M` to 5 cm and accept the bias.** Considered and rejected: this hides the systematic error rather than correcting it, and conflates "tolerance for ambiguity" with "tolerance for known bias."
- **Compensate at the consumer (drone) side.** Rejected: every consumer would have to implement the same correction, with the same risk of divergence cited in ADR-007. Compensation belongs in the producer.
- **Defer to v0.4.** Rejected: the bias exceeds the safety tolerance, which means the v0.3 system would be silently incorrect on ball positions. Implementation cost is < 50 lines of code.

## Related ADRs
- ADR-006 (FloorPlane2D World Frame) — defines the `Z = 0` floor reference.
- ADR-007 (`safe_for_control` Predicate) — the alignment tolerance this ADR protects.
- ADR-004 (Hybrid Calibration Lifecycle) — provides the per-camera intrinsics this ADR consumes.
