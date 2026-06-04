---
id: TRK-012
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-002"
spec: null
plan: null
blockers: []
---

## Context

Python calibration tool for computing per-camera intrinsic parameters (focal length, principal point, distortion coefficients) using a Charuco board. This is an offline tool (not hot-path) run once per camera at install time. Output is a versioned JSON file consumed by the tracking core at startup. Per ADR-004 Phase 1 and ADR-010, per-unit intrinsics are required for production; shared profiles are a development-only fallback.

## Acceptance

- A Python script at `tracking-core/tools/calibrate_intrinsics.py`.
- Captures N frames (configurable, default 20) of a Charuco board from the camera.
- Computes intrinsics using `cv2.aruco.calibrateCameraCharuco()`.
- Outputs a JSON file with schema: `{ "intrinsics_version": 1, "camera_id": "...", "camera_matrix": [[...]], "dist_coeffs": [...], "image_size": [w, h], "reprojection_error_px": float, "calibration_timestamp": "ISO8601" }`.
- Rejects calibration if reprojection error > 1.0 px (configurable threshold).
- Prints instructions to the operator during capture (hold board steady, cover the FoV).
- Persists to the path specified in config (`calibration.intrinsics_path`).
- Integration test: synthetic Charuco images → valid JSON output with reasonable intrinsics.

## Plan

U1. Create `tracking-core/tools/calibrate_intrinsics.py` with argument parsing (camera ID, output path, board config).
U2. Implement frame capture loop: display live preview, capture on keypress or timer.
U3. Detect Charuco corners in each captured frame using `cv2.aruco.detectMarkers()` + `cv2.aruco.interpolateCornersCharuco()`.
U4. Run `cv2.aruco.calibrateCameraCharuco()` on collected corners.
U5. Validate reprojection error against threshold; abort if too high with guidance.
U6. Write JSON output with versioned schema.
U7. Add board configuration (squares, marker size, dictionary) to `tracking_core.yaml` under `calibration.charuco.*`.
U8. Integration test: use `cv2.aruco.CharucoBoard.generateImage()` to create synthetic calibration images, run tool, verify JSON output structure.

## Log

- 2026-05-31: created. Status: backlog. Python tooling — depends on TRK-002 (requirements.txt updated) but not on C++ pipeline.
