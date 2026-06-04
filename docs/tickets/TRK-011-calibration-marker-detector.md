---
id: TRK-011
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-008"
spec: null
plan: null
blockers: []
---

## Context

The calibration marker detector identifies ArUco/Charuco markers in the camera's field of view. It serves two purposes: (1) Phase 2 health monitoring (continuous detection of a static marker for calibration drift detection per ADR-004), and (2) install-time calibration (detecting the full Charuco board for computing the floor homography). The detector uses OpenCV's ArUco module, which is well-optimised and runs within budget.

## Acceptance

- A `CalibrationMarkerDetector` class with `detect(const cv::Mat& frame) -> std::vector<MarkerObservation>`.
- `MarkerObservation` struct: `marker_id` (int), `corners_px` (array of 4 cv::Point2f), `centroid_px` (float x, y), `reprojection_quality` (float).
- Supports both individual ArUco markers and Charuco board detection.
- Uses a configurable ArUco dictionary (default: `DICT_4X4_50` for low false-positive rate).
- Sub-pixel corner refinement enabled (`cv::cornerSubPix`).
- Processing time ≤ 2 ms per frame for marker detection (ArUco is fast on small dictionaries).
- Config fields: `calibration.aruco_dictionary` (default `4X4_50`), `calibration.marker_ids` (list of expected static marker IDs for health monitoring).
- Unit tests: synthetic marker image → correct ID and corner positions; no markers → empty vector; rotated/skewed marker → still detected with correct corners.

## Plan

U1. Create `src/core/detection/calibration_marker_detector.hpp` with class and `MarkerObservation` struct.
U2. Implement detection using `cv::aruco::detectMarkers()` with configurable dictionary.
U3. Apply sub-pixel corner refinement: `cv::cornerSubPix()` on detected corners.
U4. Compute centroid as mean of 4 corners.
U5. For Charuco board mode: `cv::aruco::interpolateCornersCharuco()` for full-board calibration use.
U6. Add config fields: `calibration.aruco_dictionary`, `calibration.marker_ids`.
U7. Unit tests: synthetic ArUco marker images generated with `cv::aruco::generateImageMarker()`.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-008 (frames in detection pipeline).
