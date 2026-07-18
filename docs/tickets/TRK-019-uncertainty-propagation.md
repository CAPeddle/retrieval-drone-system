---
id: TRK-019
status: done
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-018"
spec: null
plan: null
blockers: []
---

## Context

Per ADR-010, pixel detection uncertainty propagates through the ray-plane intersection to a floor-plane position uncertainty. A 1-pixel error on the image plane maps to different floor-plane uncertainties depending on camera tilt and object distance. Far-from-camera observations have larger floor-plane uncertainty. This is critical for the safe_for_control predicate's future clause 9 (uncertainty margin) and for consumers to assess position quality.

## Acceptance

- `CoordinateMapper::project_to_floor()` returns `uncertainty_m` alongside position.
- Uncertainty computed via the projection Jacobian: `J = d(floor_pos) / d(pixel_pos)`.
- Input uncertainty: configurable `pixel_uncertainty_stddev` (default 1.0 px).
- Output: `uncertainty_m = ||J|| × pixel_uncertainty_stddev` (spectral norm or Frobenius, documented choice).
- Near-camera observations have smaller uncertainty than far-camera observations (verified in test).
- Different camera tilts produce different uncertainty patterns (verified in test).
- Processing cost negligible (Jacobian is a 2×2 matrix from the already-computed projection).
- Unit tests: known geometry → verify uncertainty values; compare near vs far observations; compare steep vs shallow camera tilt.

## Plan

U1. Compute the 2×2 Jacobian of the floor-plane projection analytically (derivative of ray-plane intersection w.r.t. pixel coordinates).
U2. Multiply Jacobian norm by configured pixel uncertainty to get floor-plane uncertainty.
U3. Store as `uncertainty_m` in `FloorPosition` struct.
U4. Add config field: `coordinate.pixel_uncertainty_stddev_px` (default 1.0).
U5. Unit tests: verify monotonic increase with distance, verify camera-angle dependence, verify specific known-geometry cases.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-018 (extends CoordinateMapper with uncertainty).
- 2026-07-18: backlog -> done. Analytic projective Jacobian x pixel stddev (spectral norm), numeric finite-difference cross-check within 10%; far>near and shallow>steep verified. Combined with track prediction uncertainty at snapshot build.
