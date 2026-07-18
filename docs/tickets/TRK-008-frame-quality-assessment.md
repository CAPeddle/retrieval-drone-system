---
id: TRK-008
status: done
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-07-17
depends_on:
  - "TRK-007"
spec: null
plan: null
blockers: []
---

## Context

Frame Quality Assessment is the first processing stage after ingestion. It gates whether a frame enters the detection pipeline or is discarded. A frame that is severely under/overexposed or motion-blurred will produce unreliable detections. Discarding it early saves CPU budget for the next good frame. This is NOT a hot-path bottleneck (the check is cheap: histogram analysis + Laplacian variance), but it prevents garbage-in propagation.

## Acceptance

- A `FrameQualityAssessor` class with a `assess(const cv::Mat& frame) -> FrameQuality` method.
- `FrameQuality` is an enum: `GOOD`, `DEGRADED`, `REJECT`.
- Assessment checks:
  - **Exposure:** histogram mean and spread. Reject if mean < `fqa.underexposed_threshold` (default 20) or mean > `fqa.overexposed_threshold` (default 240).
  - **Blur:** Laplacian variance. Reject if variance < `fqa.blur_threshold` (default 100).
- `DEGRADED` returned when values are marginal (within 20% of thresholds) — pipeline continues but logs a warning.
- Thresholds configurable via `tracking_core.yaml` under `frame_quality.*`.
- Processing time < 0.5 ms per frame on Pi 5 (Laplacian on 640×480 grayscale is fast).
- Unit tests: synthetic images with known exposure/blur characteristics produce expected quality levels.
- Rejected frames are counted in system health reporting (frames_rejected_count per second).

## Plan

U1. Create `src/core/detection/frame_quality.hpp` with `FrameQualityAssessor` class and `FrameQuality` enum.
U2. Implement exposure check: `cv::mean()` on the frame, compare against thresholds.
U3. Implement blur check: `cv::Laplacian()` then `cv::meanStdDev()` on the result; variance = stddev².
U4. Return `REJECT` if either check fails hard, `DEGRADED` if marginal, `GOOD` otherwise.
U5. Add config fields under `frame_quality`: `underexposed_threshold`, `overexposed_threshold`, `blur_threshold`.
U6. Unit tests: all-black frame → REJECT (underexposed), all-white → REJECT (overexposed), gaussian-blurred natural image → REJECT (blur), sharp image → GOOD.
U7. Integrate into pipeline: consumer thread pops frame from ring buffer, calls assessor, skips REJECT frames.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-006 (frames available in ring buffer).
- 2026-07-17: backlog → in-progress. Starting on feat/v03-frame-pipeline; TRK-005/006/007 landed.
- 2026-07-17: in-progress → done. FrameQualityAssessor landed: exposure mean + Laplacian variance gates, 20%-margin DEGRADED band, pre-allocated working buffers, rejected-frame counter for TRK-023, consumer-loop integration with 1 Hz aggregated reporting. Header at src/core/include/frame_quality.hpp per the repo's flat-include convention (ticket named src/core/detection/). 54/54 ctest green Release+Debug.
- 2026-07-17: review note: the <0.5 ms/frame Pi 5 budget in Acceptance is deferred to on-target verification (TRK-026 benchmark suite), per the cross-environment handoff recipe. Dev-machine ctest covers correctness only.
