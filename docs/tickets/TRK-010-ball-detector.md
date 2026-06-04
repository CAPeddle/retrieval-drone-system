---
id: TRK-010
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

The ball detector identifies the physical ball in each frame. For v0.3 ("laser on tabletop" scenario), the ball is a foam ball of known radius resting on or rolling across the floor. Under the NoIR camera (no IR filter), the ball appears as a blob distinguishable by size, shape (circularity), and relative brightness. The detector does NOT use temporal correlation (that's the laser's job) — it operates per-frame.

## Acceptance

- A `BallDetector` class with `detect(const cv::Mat& frame) -> std::optional<BallObservation>`.
- `BallObservation` struct: `centroid_px` (float x, y), `radius_px` (estimated from contour), `confidence`, `circularity`.
- Detection algorithm:
  1. Adaptive threshold or morphological operations to isolate candidate blobs.
  2. Contour finding on the binary image.
  3. Filter contours by: area (min/max from config), circularity (> 0.7), convexity.
  4. Select the best candidate by circularity × area score (or configurable metric).
- Size gating based on expected ball radius at known camera distance (config: `ball.expected_radius_px_min`, `ball.expected_radius_px_max`).
- Returns `std::nullopt` when no candidate passes all gates.
- Processing time ≤ 2 ms per frame on Pi 5 for 640×480.
- No heap allocations in `detect()` — contour vectors pre-allocated with `reserve()`.
- Unit tests: synthetic ball image (circle on background) → detected with correct centroid; no ball → nullopt; multiple blobs with only one circular → correct one selected.

## Plan

U1. Create `src/core/detection/ball_detector.hpp` with class and `BallObservation` struct.
U2. Implement detection pipeline:
   - Convert to grayscale if needed (NoIR is effectively mono, but ensure).
   - Apply Gaussian blur to reduce noise.
   - Adaptive threshold (or Otsu) to binary.
   - Morphological close to fill gaps.
   - `cv::findContours()` on binary image.
   - Filter by area, circularity (`4π × area / perimeter²`), convexity.
U3. Select best candidate by composite score.
U4. Compute centroid via `cv::moments()` for sub-pixel accuracy.
U5. Estimate radius from contour area: `r = sqrt(area / π)`.
U6. Add config fields: `ball.expected_radius_px_min`, `ball.expected_radius_px_max`, `ball.min_circularity` (default 0.7), `ball.detection_blur_kernel` (default 5).
U7. Pre-allocate contour vector and working Mats at construction.
U8. Unit tests: synthetic images, edge cases (ball at frame edge, partial occlusion).

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-008 (frames available post quality check).
