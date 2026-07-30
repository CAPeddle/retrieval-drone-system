---
id: TRK-009
status: backlog
subsystem: tracking-core
tier: design
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-008"
spec: null
plan: null
blockers: []
---

## Context

The modulation-based laser detector is the most architecturally significant detection component in v0.3 (ADR-005). It correlates pixel intensity over a 4-frame rolling window against the known 15 Hz modulation pattern. This eliminates false positives from sunbeams, ambient LEDs, and unmodulated reflections. The detector is stateful (holds a rolling buffer), operates in the frequency domain (PSD at modulation frequency), and must complete within the per-frame budget on Pi 5.

Key ADR-005 requirements:
- 15 Hz modulation, 50/50 duty, 4-frame window at 60 fps.
- Phase-insensitive detection (no clock synchronisation with MCU).
- Initial grace period: 2 full cycles (~134 ms) before any detections are emitted.
- Secondary disambiguation for specular reflections (brightness-based, prefer brighter cluster).

## Acceptance

- A `ModulationDetector` class with:
  - `push_frame(const cv::Mat& frame, uint64_t timestamp_ns)` — adds frame to rolling buffer.
  - `detect() -> std::vector<LaserObservation>` — returns candidate laser observations.
- Rolling buffer holds exactly 4 frames (one modulation period at 15 Hz / 60 fps).
- Detection algorithm:
  1. For each pixel (or candidate ROI), compute intensity time-series over the 4-frame window.
  2. Compute PSD at the modulation frequency (15 Hz).
  3. Threshold PSD power: pixels exceeding `laser.psd_threshold` are candidates.
  4. Cluster adjacent candidate pixels into contiguous regions.
  5. For each cluster, compute centroid (sub-pixel) and peak brightness.
  6. If multiple clusters pass, select the brightest (secondary disambiguation per ADR-005 R-04).
- Grace period: first `2 * (fps / modulation_freq)` frames produce no detections (returns empty).
- `LaserObservation` struct: `centroid_px` (float x, y), `confidence` (PSD power normalised), `cluster_size_px`, `peak_brightness`.
- Processing time ≤ 4 ms per frame on Pi 5 for 640×480 (budgeted in the 16.6 ms frame budget).
- Unit tests with synthetic modulated signal: known 15 Hz pattern → detected; static bright spot → not detected; two modulated clusters → brighter selected.
- No heap allocation in `detect()` — pre-allocated working buffers at construction.

## Plan

Tier `design` — this ticket spawns sub-tickets for its major sub-work. The overseer agent tracks progress across these children.

### Sub-tickets

| Child | Title | Scope |
|-------|-------|-------|
| TRK-009a | PSD strategy benchmark spike | Benchmark full-frame DFT vs ROI-gated approach on Pi 5; decide strategy |
| TRK-009b | Rolling buffer + correlation engine | 4-frame buffer, PSD computation, threshold gating |
| TRK-009c | Clustering + centroid extraction | Connected-component labelling, sub-pixel centroid, secondary disambiguation |
| TRK-009d | Grace period + integration | Startup suppression, pipeline integration, end-to-end unit tests |

### Orchestration notes for overseer agent

- TRK-009a must complete first — its output (strategy choice) gates TRK-009b's implementation.
- TRK-009b and TRK-009c are implementation-independent but TRK-009c consumes TRK-009b's correlation map output.
- TRK-009d integrates the whole detector into the pipeline and runs final validation.
- All children share this parent's Acceptance criteria — the parent is Done only when all children pass AND the parent's acceptance holds.

## Log

- 2026-05-31: created. Status: backlog. Tier is `design` because the PSD computation strategy (full-frame vs ROI-gated) is a non-trivial performance trade-off requiring a benchmark spike. Depends on TRK-008 (frames enter detection after quality check).
- 2026-07-21: **TRK-009a PSD strategy benchmark — DECISION: Option A (full-frame DFT).** Measured on the Pi 5 (aarch64, `build-release`) via `benchmarks/psd_strategy_bench` over the shared synthetic fixture (U1). Window = 8 frames (two modulation periods, KTD-1 deviation from the ticket's original 4-frame window). On-target Release numbers (dev-host timings are not evidence — production-binary-oracle learning):

  | Strategy | overall mean | overall p99 |
  |---|---|---|
  | A — full-frame 8-point DFT | 5.10 ms | **5.52 ms** |
  | B — brightness-prefiltered | 2.30 ms | 5.92 ms |

  Both meet every expected synthetic outcome (detect the three true-dot amplitudes incl. dim-at-floor and the bright-clutter dim dot; reject step / moving-edge / empty). B is admissible (matches A everywhere A is correct) and ~2.2× lower on *mean*, but **its p99 is not lower than A's**: the bright adversarial scenes (light-switch step, translating edge) render the brightness prefilter useless (whole frame becomes candidate), so B collapses to A's cost with extra tail jitter exactly where the per-frame budget is tightest. Per the KTD-4 rule (among admissible strategies choose lower p99; tie/ambiguity → A) the choice is **A** — also simpler, with no prefilter threshold to re-provision on real footage. A's admissibility-independence means the provisional Option-B prefilter (30, PROVISIONAL) does not affect the outcome.

  **Execution signal for TRK-009b/d (not a gate here):** the benchmark's per-pixel *scalar* 8-tap DFT costs ~5 ms/window on the Pi — above the R10 ≤4 ms detect-cycle budget. That is the fair model for the A-vs-B pixel-set comparison, not the shipped detector: U3 implements Option A with the four-phase accumulation into pre-allocated float Mats (whole-Mat weighted sums, `re += coef·frame`), which removes the scalar per-pixel overhead. The R10 budget is proven against that vectorised implementation at the Phase C perf gate, not against this reference binary. Thresholds in the benchmark (`power_min`, `purity_min`, `prefilter`) are PROVISIONAL pending the operator's modulated-laser recording session (R14).
- 2026-07-22: **review hardening + Pi re-run — Option A decision CONFIRMED under stricter evidence.** Multi-agent code review of the Phase A PR found the original correctness evidence for `step_offset8` was vacuous (the evaluated steady-state window lay entirely after the transition — a uniform bright field) and the positive criterion could not fail on a spurious extra cluster. Benchmark hardened: negatives are now evaluated over **every** sliding window (as the window slides across the transition the step occupies every in-window offset — the step-at-every-offset check) and must additionally show a **zero-pixel raw mask** in each, so the cluster-area cap cannot silently absorb a mass false-positive; positives now require **exactly one** truth-located cluster (matching KTD-6 fail-closed). Re-run on the Pi 5 (`build-release`): A mean 5.11 ms / p99 5.57 ms; B mean 2.30 ms / p99 5.92 ms; every negative window mask_px = 0 for both strategies; every positive exactly 1 cluster at truth. **Decision unchanged: Option A** (B admissible but p99 not lower; rule → A). Caveat on record (review finding): `connectedComponentsWithStats` output Mats are not pre-sized, so p99 samples include minor allocator jitter — symmetric across both strategies, so it does not affect the comparison, but the absolute p99 figures carry that noise. The fixture's six adversarial generators also gained known-answer self-checks (suite 8 → 14 tests).
