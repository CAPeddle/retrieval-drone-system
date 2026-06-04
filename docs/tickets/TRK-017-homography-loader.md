---
id: TRK-017
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-013"
spec: null
plan: null
blockers: []
---

## Context

The homography loader reads the per-camera extrinsic calibration JSON (produced by TRK-013) at startup and provides the `H_i` matrix to the CoordinateMapper. It validates the JSON schema version, checks that the homography is reasonable (determinant, condition number), and exposes the floor anchor points for health monitoring. This runs at startup (not hot path) but the resulting matrix is used on every frame.

## Acceptance

- A `HomographyLoader` class/function that reads the extrinsics JSON and returns a validated `cv::Matx33d` homography.
- Validates: JSON schema version matches expected, matrix is 3×3, determinant is non-zero and positive, condition number is below threshold (degenerate rejection per ADR-006).
- Exposes `floor_anchor_points` for calibration health monitoring (TRK-024).
- Returns an error (not exception on hot path — startup code may throw) if file is missing, malformed, or fails validation.
- Config consumed: `calibration.extrinsics_path`.
- Unit test: valid JSON → correct matrix; missing file → error; bad determinant → error; wrong schema version → error.

## Plan

U1. Create `src/core/coordinate/homography_loader.hpp` with loader function signature.
U2. Implement JSON parsing using nlohmann/json: read file, extract `homography_3x3`, `floor_anchor_points`, `extrinsics_version`.
U3. Validate schema version (must equal 1 for v0.3).
U4. Validate matrix properties: determinant > 0, condition number < threshold (default 1000).
U5. Return structured result: `HomographyData { cv::Matx33d H; std::vector<FloorAnchor> anchors; float reprojection_error_m; }`.
U6. Unit tests: valid/invalid JSON files, mathematical validation edge cases.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-013 (produces the JSON this consumes) and TRK-002 (nlohmann/json available).
