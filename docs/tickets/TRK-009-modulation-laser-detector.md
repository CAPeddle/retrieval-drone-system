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
