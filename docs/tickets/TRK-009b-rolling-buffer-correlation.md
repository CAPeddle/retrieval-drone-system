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
- 2026-07-22: implemented as Phase B of the modulation-detector plan (`docs/plans/2026-07-21-001`, units U3/U5). `ModulationDetector` (`src/core/tracking/modulation_detector.cpp`) with rolling window, contiguity + 1.5x-period gap discipline (KTD-3), and Option-A full-frame DFT (TRK-009a decision) implemented as four-phase whole-Mat accumulation into pre-allocated float Mats. **KTD-1 deviation from this ticket's text: the window is 8 frames (two modulation periods), not the 4 the parent acceptance sketches** — a 4-point DFT cannot distinguish a luminance step from one modulation period (structural false-SAFE); recorded in the ADR-005 amendment (U9, drafted, pending user sign-off). Config gained the KTD-8 laser fields; the single `laser.psd_threshold` this ticket's plan named is superseded by `psd_power_min` + `psd_purity_min` (KTD-2 two-gate: power floor AND two-sided purity floor, structural minimum > 0.4). Window derives `2 * target_fps / f_mod` (validated integral >= 8), not a config field. Rejection suite (step at every offset, translating edge, 20 Hz off-bin) green; on-bin 45 Hz alias detected as expected (R3). Frame-aligned replay-wrap marker (`FrameSource::consume_wrap()` → `FrameMetadata.wrapped`) landed for R4. Status left `backlog` — TRK-009* transitions batched at the plan's Phase C close-out (U8).
- 2026-07-23: **U9 ADR-005 amendment ACCEPTED (user sign-off).** Two-period window accepted. **Reaction-bound decision: accept ~117 ms as the provisional v0.3 flip-to-false bound** (replay gate at 120 ms, R13), tightening deferred to the post-recording threshold re-provenance; if later tightened, the purity-floor route is preferred over `AGE_MAX_MS` (which ADR-007 shares between the laser and ball freshness clauses — a laser-only split would be an ADR-007 change). Full rationale in the ADR-005 amendment. Unblocks the Phase B PR (#20) for merge.
- 2026-07-26: acceptance amended under ADR-011 D7, no status change. Per-row temporal alignment is now the designated R-03 closure, with a falsification test (ADR-011 gate G4). Frame format becomes `CV_8UC1` per ADR-011 D5.
- 2026-07-30: master (ADR-011, PR #21) merged into the Phase B branch. Two interactions recorded, **neither resolved by Phase B**:
  1. **U7/U8 (per-row temporal alignment) are new scope added after the Phase B implementation** and are NOT implemented. `ModulationDetector::correlate_and_extract` treats each frame as instantaneous — the DFT coefficients are indexed by frame number `n`, with no row-dependent phase term. This is the pre-alignment baseline the G4 falsification test is meant to measure against, so it is a legitimate starting point, but the ticket is not complete on its amended acceptance. Blocked on `camera.readout_time_us`, which TRK-034 measures.
  2. **The `CV_8UC1` "do not add a `cvtColor`" instruction is not yet actionable.** ADR-011 is `Proposed`, TRK-033 (greyscale migration) is not done, and the rest of the hot path (`ball_detector`, `calibration_marker_detector`, `frame_quality`) still converts from `CV_8UC3`. `push_frame` therefore keeps a `cvtColor` guarded on `channels() == 3` (`modulation_detector.cpp:83-84`); a single-channel frame already bypasses it, so the branch becomes dead the moment TRK-033 lands and costs nothing until then. Removing it now would break every existing colour-path test. Flagged for TRK-033 to delete.
