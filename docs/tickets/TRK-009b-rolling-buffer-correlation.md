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
- **Per-row temporal alignment (R-03 closure, added 2026-07-26 under ADR-011 D7).** A pixel's true capture instant is not the frame timestamp: on a rolling shutter it is `t_frame + (row / height) × readout_time`. The correlation must apply this per-row offset rather than treating the frame as instantaneous. ADR-011 D4 reduces `readout_time` to ~8.3 ms by mode selection, but does not eliminate it, and ADR-005 names per-row alignment as the designated resolution.
  - `readout_time` is configurable, not hardcoded — it is a sensor-mode property, measured by TRK-034.
  - **Falsification test (ADR-011 gate G4):** correlation strength with the laser near the top of frame versus near the bottom, measured before and after alignment. Report both. A position-dependent gap that survives alignment does not get papered over — it escalates to a global-shutter sensor per ADR-011 Alternatives.
- Frames arrive as `CV_8UC1` (ADR-011 D5) — do not add a `cvtColor`.

## Plan

U1. Create `src/core/detection/modulation_detector.hpp` — class declaration with rolling buffer and `LaserObservation` struct.
U2. Implement rolling buffer: `std::array<cv::Mat, 4>` pre-allocated at construction, circular write index.
U3. Implement `compute_correlation_map()` using TRK-009a's selected strategy.
U4. Apply threshold to correlation map → binary candidate mask.
U5. Add config field: `laser.psd_threshold` (default TBD from TRK-009a spike results).
U6. Unit tests with synthetic temporal sequences — verify detection of modulated signals and rejection of static/wrong-frequency signals.
U7. Add config field: `camera.readout_time_us` (measured by TRK-034); apply per-row temporal alignment in `compute_correlation_map()`.
U8. R-03 falsification test: synthetic sequences with the modulated spot at the top, middle and bottom of frame; assert correlation strength is position-independent within tolerance after alignment, and record the pre-alignment gap for comparison.

## Log

- 2026-05-31: created. Status: backlog. Blocked on TRK-009a (strategy choice).
- 2026-07-26: acceptance amended under ADR-011 D7, no status change. Per-row temporal alignment is now the designated R-03 closure, with a falsification test (ADR-011 gate G4). Frame format becomes `CV_8UC1` per ADR-011 D5.
