---
id: TRK-009a
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

Child of TRK-009. The modulation detector's PSD computation has two viable strategies:
- **Option A (full-frame DFT):** compute temporal DFT for every pixel across the 4-frame window. Clean, no pre-filtering, but expensive (640×480 × 4-point DFT per frame).
- **Option B (ROI-gated):** threshold each frame for bright pixels first, then compute temporal correlation only on pixels that exceed brightness threshold in ≥1 frame. Much cheaper on frames with few bright pixels, but adds a brightness pre-filter that could miss dim laser dots.

This spike benchmarks both on Pi 5 hardware and selects the strategy for TRK-009b.

## Acceptance

- A benchmark script/executable that runs both approaches on representative 640×480 grayscale frames.
- Timing results on Pi 5: mean and p99 per-frame cost for each approach.
- Decision documented: which approach is selected and why (raw timing + correctness trade-off).
- If Option B is selected: document the brightness pre-threshold value and its relationship to expected laser intensity under typical conditions.
- Result captured in a brief note in `docs/tickets/TRK-009-modulation-laser-detector.md` Log section.

## Plan

U1. Create a standalone benchmark binary (`benchmarks/psd_strategy_bench.cpp`) that loads synthetic 4-frame sequences with known modulated signals.
U2. Implement Option A: temporal DFT per-pixel (4-point DFT = dot product with complex exponential at 15 Hz).
U3. Implement Option B: brightness threshold pre-filter → temporal correlation on surviving pixels only.
U4. Run both on Pi 5 with representative frame data (or synthetic frames matching expected scene statistics).
U5. Compare: timing (must be ≤ 4 ms for the winner), correctness on dim signals, false-positive rate on static bright regions.
U6. Document decision in parent ticket log and update TRK-009b's plan with the chosen strategy.

## Log

- 2026-05-31: created. Status: backlog. First child of TRK-009; gates implementation strategy.
