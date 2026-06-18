# ADR-006: FloorPlane2D World Frame

## Status
Accepted (2026-05-08). Supersedes the v0.2 implicit "tabletop plane" assumption.

## Context
v0.2 specified a `Plane2D_World` coordinate space whose physical interpretation was left informal ("table or floor"). During v0.3 design, two facts forced clarification:

1. The deployment camera geometry is "tabletop-mounted, pointed downwards at the floor where the ball rolls." This is an oblique (not top-down) view of the floor. A camera-relative pixels-per-meter scalar does not survive perspective foreshortening.
2. v0.3 must ship as multi-camera scaffolding (single camera now, ≥ 2 cameras in Phase 3+). Two cameras with two camera-egocentric frames cannot be fused into a single world position without a shared frame.

A consumer-relative or camera-relative frame fails both requirements. A shared external frame, anchored to a stable physical reference, addresses both.

Forces:
- The floor is the physical surface of interest (laser hits floor; ball rests / rolls on floor).
- The floor is approximately planar at room scale (cm-precision) and shared by all cameras.
- A homography mapping pixels → floor is straightforward to compute per-camera given known floor anchors.
- The frame must survive a single camera being added or removed without re-defining the frame.

## Decision
The shared world frame for v0.3 is **FloorPlane2D**, defined as follows:

- **Origin:** the corner of the primary calibration Charuco board at install time.
- **Orientation:** right-handed.
- **X axis:** along the long edge of the calibration Charuco board.
- **Y axis:** perpendicular to X, in the floor plane.
- **Z axis:** perpendicular to the floor, positive upward.
- **Units:** metres.
- **Z constraint in v0.3:** all `Plane2D_World` positions report `z_m` as the object-class-expected height per ADR-010. The floor itself is `z_m = 0`.

Each camera carries its own per-camera homography `H_i` mapping image pixels to FloorPlane2D coordinates. The homography is computed at install time using OpenCV `cv2.findHomography()` with at least 4 known floor anchor points (Charuco corners physically placed on the floor). Anchors must be spatially distributed across the camera's field of view; degenerate near-collinear configurations are rejected at calibration time.

The FloorPlane2D frame is invariant under camera additions or removals. Adding a second camera in Phase 3 requires only computing `H_2` for that camera; the frame definition does not change.

## Consequences

**Positive:**
- A single, well-defined world frame is shared across all cameras and all consumers (viewer, MAVLink adapter, replay logger).
- The drone's onboard EKF can consume positions in this frame after a single transformation to NED (handled by the MAVLink adapter, not by the core).
- Multi-camera scaffolding works trivially: each camera maps independently into the shared frame and observations are fused there.
- Calibration health is well-defined: a static AprilTag at a known floor position is observed; its reprojected coordinate is compared with its known FloorPlane2D coordinate; deviation drives `calibration_status`.

**Negative:**
- The Charuco corner that defines the origin is a physical location that must be marked or recorded; if it is moved or lost, re-establishing the frame requires re-running the install procedure for all cameras.
- The frame assumes the floor is approximately planar. Local non-planarity (rugs, thresholds, slopes) introduces position bias that scales with the local floor height variation. Documented as a known systematic error.
- Per-camera homography drift means each camera has its own calibration health independent of the others.

**Risks:**
- **Floor non-planarity:** a 5 mm rug edge or threshold introduces a bias of comparable magnitude in floor coordinates. This is below the `safe_for_control` alignment tolerance of 20 mm (per ADR-007), but consumes part of that budget. If the operating area has known floor irregularities, they should be marked as exclusion zones in configuration.
- **Origin loss:** if the calibration Charuco's install location is undocumented, re-calibration requires guessing. Mitigated by persisting the install-time floor coordinate of every static AprilTag in the calibration JSON, so the frame can be re-derived from any surviving anchor.
- **Camera pointed at non-floor surface:** if a camera is accidentally pointed at a wall or ceiling, the homography fit will succeed mathematically but produce nonsense world coordinates. Mitigated by reprojection-error gating at calibration time (`reprojection_error_px > 3.0` rejects the calibration).

## Alternatives Considered
- **Camera-relative frame with `pixels_per_meter` scalar.** Considered and rejected during v0.3 design discussion: does not survive oblique viewing, does not scale to multi-camera, requires re-definition every time a camera moves.
- **Drone-body frame.** Rejected: the drone is one of the *tracked objects*; making the drone the frame origin introduces a chicken-and-egg dependency.
- **Room-anchored 3D frame.** Considered for future phases: a six-degree-of-freedom world frame anchored to multiple wall-mounted AprilTags would generalise FloorPlane2D to full 3D. Out of scope for v0.3.

## Related ADRs
- ADR-003 (Graceful Coordinate Degradation) — defines `Plane2D_World` as the v0.3 coordinate space realised by this ADR.
- ADR-004 (Hybrid Calibration Lifecycle) — specifies the calibration procedure that produces `H_i` for each camera.
- ADR-010 (Object-Class Z Compensation) — describes how `z_m` is determined per object type within this frame.
