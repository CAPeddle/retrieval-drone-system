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
