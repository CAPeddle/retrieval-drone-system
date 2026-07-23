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
- 2026-07-22: implemented as Phase B of the modulation-detector plan (`docs/plans/2026-07-21-001`, units U3/U5). `ModulationDetector` (`src/core/tracking/modulation_detector.cpp`) with rolling window, contiguity + 1.5x-period gap discipline (KTD-3), and Option-A full-frame DFT (TRK-009a decision) implemented as four-phase whole-Mat accumulation into pre-allocated float Mats. **KTD-1 deviation from this ticket's text: the window is 8 frames (two modulation periods), not the 4 the parent acceptance sketches** — a 4-point DFT cannot distinguish a luminance step from one modulation period (structural false-SAFE); recorded in the ADR-005 amendment (U9, drafted, pending user sign-off). Config gained the KTD-8 laser fields; the single `laser.psd_threshold` this ticket's plan named is superseded by `psd_power_min` + `psd_purity_min` (KTD-2 two-gate: power floor AND two-sided purity floor, structural minimum > 0.4). Window derives `2 * target_fps / f_mod` (validated integral >= 8), not a config field. Rejection suite (step at every offset, translating edge, 20 Hz off-bin) green; on-bin 45 Hz alias detected as expected (R3). Frame-aligned replay-wrap marker (`FrameSource::consume_wrap()` → `FrameMetadata.wrapped`) landed for R4. Status left `backlog` — TRK-009* transitions batched at the plan's Phase C close-out (U8).
- 2026-07-23: **U9 ADR-005 amendment ACCEPTED (user sign-off).** Two-period window accepted. **Reaction-bound decision: accept ~117 ms as the provisional v0.3 flip-to-false bound** (replay gate at 120 ms, R13), tightening deferred to the post-recording threshold re-provenance; if later tightened, the purity-floor route is preferred over `AGE_MAX_MS` (which ADR-007 shares between the laser and ball freshness clauses — a laser-only split would be an ADR-007 change). Full rationale in the ADR-005 amendment. Unblocks the Phase B PR (#20) for merge.
