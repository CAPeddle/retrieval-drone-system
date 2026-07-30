---
id: TRK-009c
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-009b"
spec: null
plan: null
blockers:
  - "TRK-009b must complete (correlation map available)"
---

## Context

Child of TRK-009. Takes the binary candidate mask from TRK-009b's correlation engine and extracts structured `LaserObservation`s: connected-component clustering, sub-pixel centroid computation, and secondary disambiguation when multiple clusters pass (per ADR-005's specular-reflection handling, R-04).

## Acceptance

- Connected-component labelling on the binary candidate mask identifies distinct clusters.
- Clusters filtered by size: `laser.min_cluster_size_px` ≤ area ≤ `laser.max_cluster_size_px`.
- Per-cluster centroid computed as PSD-power-weighted mean of pixel positions (sub-pixel accuracy).
- Per-cluster peak brightness recorded from the latest raw frame.
- Secondary disambiguation: if >1 cluster passes size gate, select the one with highest peak brightness.
- Returns `std::vector<LaserObservation>` (typically 0 or 1 elements; vector for forward-compat).
- Pre-allocated label map and stats buffers — no heap allocation in `detect()`.
- Unit tests: single cluster → single observation; two clusters → brighter selected; cluster too small → rejected; cluster too large → rejected.

## Plan

U1. Add clustering method to `ModulationDetector`: `cv::connectedComponentsWithStats()` on the thresholded correlation map.
U2. Filter components by area against config thresholds.
U3. Compute PSD-weighted centroid for each surviving cluster.
U4. Record peak brightness from the latest raw frame within each cluster's bounding box.
U5. Implement secondary disambiguation: sort by peak brightness, return top candidate.
U6. Add config fields: `laser.min_cluster_size_px` (default 3), `laser.max_cluster_size_px` (default 500).
U7. Unit tests: multi-cluster scenarios, size filtering, disambiguation by brightness.

## Log

- 2026-05-31: created. Status: backlog. Blocked on TRK-009b (needs correlation map).
- 2026-07-22: implemented as Phase B of the modulation-detector plan (`docs/plans/2026-07-21-001`, unit U4). Extraction stage in `ModulationDetector::correlate_and_extract`: morph-close (3x3 ellipse) → `connectedComponentsWithStats` → area filter `[laser.min_cluster_size_px, laser.max_cluster_size_px]` → PSD-power-weighted sub-pixel centroid → ON-frame peak brightness (ON phase from the DFT argument at the peak pixel). **KTD-6 deviation from this ticket's text: multi-cluster ambiguity is FAIL-CLOSED — more than one surviving cluster emits NO observation and increments `ambiguous_detections()` (surfaced in system_health), replacing this ticket's "select the brightest" secondary disambiguation.** Rationale: a modulated specular ghost is indistinguishable from the laser by PSD (R-04), so brightest-wins is a false-SAFE contributor; fail-closed (real laser + ghost ⇒ unsafe) is the correct direction. `LaserObservation` carries centroid, psd_power, purity, peak_brightness, cluster_area (feeds the U6 radius = sqrt(area/pi) mapping), and the newest-frame timestamp. Config default names align with this ticket's plan (`min_cluster_size_px`/`max_cluster_size_px`). Unit tests: single-cluster centroid within 1 px of truth, two-cluster fail-closed + counter, area under/over gates, saturated-bloom centroid bias bounded, ON-frame peak brightness. Status left `backlog` — batched at Phase C close-out (U8).
