---
id: TRK-013
status: done
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-07-18
depends_on:
  - "TRK-012"
spec: null
plan: null
blockers: []
---

## Context

Python calibration tool for computing the per-camera homography from image pixels to the FloorPlane2D world frame (ADR-006). The operator places a Charuco board (or individual markers) at known floor positions and the tool computes `H_i` (3×3 homography matrix). This homography is consumed by the C++ CoordinateMapper at runtime. Per ADR-006: at least 4 spatially distributed floor anchor points required; degenerate near-collinear configurations are rejected.

## Acceptance

- A Python script at `tracking-core/tools/calibrate_extrinsics.py`.
- Operator places calibration markers at known floor positions (at least 4 points, distributed across FoV).
- Tool captures marker positions in image space and accepts known floor coordinates (typed or from a calibration layout file).
- Computes homography via `cv2.findHomography()` with RANSAC.
- Validates: at least 4 points, not near-collinear (condition number check), reprojection error < `CAL_HEALTH_FAIL_M` (default 0.020 m per ADR-004).
- Outputs JSON: `{ "extrinsics_version": 1, "camera_id": "...", "homography_3x3": [[...]], "floor_anchor_points": [...], "reprojection_error_m": float, "calibration_timestamp": "ISO8601" }`.
- Rejects if fewer than 4 points or if spatial distribution is degenerate.
- Integration test: synthetic known-geometry setup → correct homography within tolerance.

## Plan

U1. Create `tracking-core/tools/calibrate_extrinsics.py` with argument parsing (camera ID, output path, anchor layout file).
U2. Implement anchor point collection: either from a layout JSON file (pre-measured positions) or interactive (operator clicks image + types floor coords).
U3. Detect markers in camera view, match to known floor positions.
U4. Compute homography with `cv2.findHomography(src_pts, dst_pts, cv2.RANSAC)`.
U5. Validate point count (≥ 4), spatial distribution (condition number of point matrix), reprojection error.
U6. Write JSON output with versioned schema.
U7. Add config reference: `calibration.extrinsics_path` (already in TRK-003 config).
U8. Integration test: synthetic point correspondences with known homography → tool produces matching H within tolerance.

## Log

- 2026-05-31: created. Status: backlog. Python tooling — depends on TRK-012 (intrinsics available for undistortion during anchor detection).
- 2026-07-18: backlog → in-progress. Starting on feat/v03-detection-calibration; TRK-012 intrinsics landed.
- 2026-07-18: in-progress → done. calibrate_extrinsics.py landed: image->FloorPlane2D homography via findHomography(method=0, NOT RANSAC — dst units are metres, a bad anchor must fail the gate not be masked as outlier), error computed over ALL anchors. Gates: >=4 (warn exact-fit at 4), non-collinear SVD sigma2/sigma1<1e-3, <=0.020m (CAL_HEALTH_FAIL_M), <=3.0px (ADR-006). Undistorts via --intrinsics; JSON records undistorted_coordinates:true + floor_anchor_points with image px (frame re-derivable per ADR-006). 6 tests incl. exact+near-collinear(2mm) and perturbed-anchor rejection; 14/14 python green, black+ruff clean. Interactive click mode out of scope (KTD-5, layout file replaces it).
