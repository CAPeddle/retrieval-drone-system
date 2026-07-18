---
id: TRK-011
status: done
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-07-18
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
- Supports individual ArUco markers. (Charuco *board* detection was deliberately deferred to the Python calibration tools per the plan KTD-2; the C++ runtime consumer — ADR-004 Phase 2 health monitoring, TRK-024 — needs only individual markers. See Log 2026-07-18.)
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
- 2026-07-18: backlog → in-progress. Starting on feat/v03-detection-calibration.
- 2026-07-18: in-progress → done. CalibrationMarkerDetector landed: KTD-2 version seam (guarded includes core/version.hpp->aruco.hpp|objdetect/aruco_detector.hpp + CMake aruco/objdetect component selection), 4.6 free-fn branch tested locally (6/6), 4.10 ArucoDetector branch verifies on Pi via pi5-remote-test. Individual markers only; Charuco board mode deliberately deferred to Python tools (KTD-2 scope note) — C++ runtime consumer (ADR-004 Phase2 health, TRK-024) needs individual markers. Field renamed reprojection_quality->corner_residual_px (honest naming). New require_int_list config helper; calibration.charuco.* geometry (odd squares). 78/78 green Release+Debug.
