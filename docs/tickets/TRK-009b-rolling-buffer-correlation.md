---
id: TRK-009b
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-009a"
spec: null
plan: null
blockers:
  - "TRK-009a must complete (strategy decision)"
---

## Context

Child of TRK-009. Implements the rolling buffer and temporal correlation engine using the strategy selected by TRK-009a. This is the core signal-processing component of the modulation detector.

## Acceptance

- A `ModulationDetector` class with a 4-frame rolling buffer (circular, pre-allocated).
- `push_frame(const cv::Mat& frame, uint64_t timestamp_ns)` adds to buffer.
- Internal `compute_correlation_map()` produces a single-channel float Mat where each pixel's value is the PSD power at the modulation frequency.
- Correlation map is thresholded at `laser.psd_threshold` (from config) to produce a binary candidate mask.
- No heap allocations after construction.
- Unit tests: synthetic 15 Hz modulated signal → high PSD; static signal → low PSD; 30 Hz signal → low PSD (wrong frequency rejected).

## Plan

U1. Create `src/core/detection/modulation_detector.hpp` — class declaration with rolling buffer and `LaserObservation` struct.
U2. Implement rolling buffer: `std::array<cv::Mat, 4>` pre-allocated at construction, circular write index.
U3. Implement `compute_correlation_map()` using TRK-009a's selected strategy.
U4. Apply threshold to correlation map → binary candidate mask.
U5. Add config field: `laser.psd_threshold` (default TBD from TRK-009a spike results).
U6. Unit tests with synthetic temporal sequences — verify detection of modulated signals and rejection of static/wrong-frequency signals.

## Log

- 2026-05-31: created. Status: backlog. Blocked on TRK-009a (strategy choice).
