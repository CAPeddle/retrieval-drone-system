---
id: TRK-033
status: backlog
subsystem: tracking-core
tier: small
created: 2026-07-26
updated: 2026-07-26
depends_on: ["TRK-032"]
spec: null
plan: null
blockers: []
---
## Context

The pipeline is monochrome end to end but carries colour frames. `BallDetector::detect` (`tracking/ball_detector.cpp:35`), `CalibrationMarkerDetector` (`tracking/calibration_marker_detector.cpp:65,84`) and `FrameQuality::assess` (`frame_quality.cpp:42`) each open with `cv::cvtColor(frame, gray_, cv::COLOR_BGR2GRAY)`; nothing downstream reads a colour channel, and ADR-005 already rejects colour-based laser detection. Every frame therefore pays for a debayer, then three greyscale conversions, and only the luminance is used. ADR-011 D5 changes the `FrameSource` contract to `CV_8UC1` and has `LibcameraSource` pass the YUV420 Y plane through directly. This ticket is that migration. It is a breaking contract change across the frame source, the ring buffer, all three detectors and the replay source, and must land as one coherent change rather than incrementally.

## Acceptance

- `FrameSource`'s documented contract is `CV_8UC1`; the `grab(cv::Mat&)` signature is unchanged.
- `FrameRingBuffer` pre-allocates `CV_8UC1` slots. Its existing type-mismatch rejection (`frame_ring_buffer.hpp:58-64`) is exercised by a test that pushes a `CV_8UC3` frame and asserts rejection — the guard must be proven, not assumed.
- `BallDetector`, `CalibrationMarkerDetector` and `FrameQuality` no longer call `cvtColor` and no longer own a `gray_` scratch buffer.
- `ReplaySource` decodes recorded clips to greyscale, so replay and live capture present byte-identical data to the detectors for the same scene.
- **Existing OV5647 recordings still replay and still produce the same detections** as before the migration, within tolerance. This is the regression that proves the change is format-only and not behavioural.
- Measured CPU delta on the Pi 5, before and after, reported in the Log. ADR-011 predicts a reduction; report what is actually measured, including if it is negligible.
- Both build configs green under `tools/pi5-remote-test.sh`.

## Plan

1. **U1** — Read pass: enumerate every `CV_8UC3`, `cvtColor` and `gray_` site across `tracking-core/src`. The known set is `main.cpp:146,166`, `frame_ring_buffer.hpp:40`, `ball_detector.cpp:24-35`, `calibration_marker_detector.cpp:65,84`, `frame_quality.cpp:16,42` — confirm it is complete before editing.
2. **U2** — Write the failing regression first: a replay test asserting detector output on an existing OV5647 clip. It must pass before the migration and after it. Per CLAUDE.md §5, if it passes but reality is wrong, the test is wrong.
3. **U3** — Update the `FrameSource` contract comment in `include/frame_source.hpp`.
4. **U4** — `FrameRingBuffer` slots to `CV_8UC1`; add the type-mismatch rejection test.
5. **U5** — `ReplaySource` decodes to greyscale.
6. **U6** — Strip `cvtColor` and `gray_` from `FrameQuality`, then `BallDetector`, then `CalibrationMarkerDetector`, running U2 after each.
7. **U7** — `main.cpp` frame allocation to `CV_8UC1`.
8. **U8** — Measure CPU and latency on the Pi 5 before/after; record both in the Log.

## Log

- 2026-07-26: created. Status: backlog. Implements ADR-011 D5. Depends on TRK-032 so there is a live `CV_8UC1` source to validate against rather than replay alone.
