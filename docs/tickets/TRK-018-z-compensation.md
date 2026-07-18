---
id: TRK-018
status: done
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-017"
spec: null
plan: null
blockers: []
---

## Context

Per ADR-010, projecting pixel observations onto the floor plane requires object-class-aware Z compensation. A naive homography multiplication (`H × pixel`) assumes Z=0 (correct for laser, wrong for ball). The correct approach: compute the camera ray through the pixel using inverse intrinsics, then intersect with the plane `Z = expected_height(object_type)`. This eliminates the ~3 cm systematic bias on ball positions that would otherwise exceed the safe_for_control alignment tolerance.

## Acceptance

- A `CoordinateMapper` class with `project_to_floor(const cv::Point2f& pixel, ObjectType type) -> FloorPosition`.
- `FloorPosition` struct: `x_m`, `y_m`, `z_m`, `uncertainty_m`.
- Object-class Z lookup: Laser → 0, Ball → `ball.radius_m`, CalibrationMarker → 0.
- Algorithm: undistort pixel → back-project ray using inverse intrinsics → intersect ray with plane Z=expected_height → transform to FloorPlane2D frame.
- Requires both intrinsics (from TRK-012 JSON) and extrinsics/homography (from TRK-013 JSON).
- Intrinsics loaded at startup; must refuse to enter RUNNING state without valid intrinsics (ADR-010).
- Processing cost ≤ 0.1 ms per observation (matrix multiply + ray-plane intersection).
- No heap allocations — all matrices pre-computed at startup.
- Unit tests: laser at known pixel → Z=0 floor position; ball at known pixel → Z=radius position; verify bias correction magnitude matches ADR-010's 3 cm example geometry.

## Plan

U1. Create `src/core/coordinate/coordinate_mapper.hpp` with `CoordinateMapper` class and `FloorPosition` struct.
U2. Load and cache camera intrinsics (camera matrix + distortion coefficients) at construction from JSON.
U3. Implement `undistort_point()`: `cv::undistortPoints()` for single point.
U4. Implement ray back-projection: `ray_dir = K_inv × [u, v, 1]^T` (normalised).
U5. Implement ray-plane intersection: solve for `t` where `ray_origin + t * ray_dir` has `Z = expected_height`.
U6. Transform intersection point to FloorPlane2D using the extrinsic homography.
U7. Add ObjectType-to-height lookup table (config-driven for ball radius).
U8. Pre-compute inverse intrinsics matrix at construction.
U9. Unit tests: known geometry scenarios matching ADR-010's worked example.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-017 (homography loaded) and TRK-012 (intrinsics available).
- 2026-07-18: backlog -> done. CoordinateMapper landed: camera centre via plane-pose extraction B=K^-1 H^-1 (NOT decomposeHomographyMat — wrong contract for a world-plane homography), closed-form height scaling, degenerate rejection (valid=false, never NaN). ADR-010 worked-example gate: naive >2cm bias, compensated <=5mm.
