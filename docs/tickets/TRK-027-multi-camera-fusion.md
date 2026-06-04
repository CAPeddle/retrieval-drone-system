---
id: TRK-027
status: backlog
subsystem: tracking-core
tier: design
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "CAM-001"
spec: null
plan: null
blockers: []
---

## Context

Phase 3 introduces multi-camera fusion. The tracker receives observations from N cameras (N=2 initially: local Pi 5 CSI + remote Pi 3B stream). Each camera has its own intrinsics and extrinsics (homography to FloorPlane2D). Observations from all cameras project into the shared floor frame (ADR-006) and are fused in the tracker.

Design considerations:
- v0.3 already scaffolds for N cameras at the observation/track interface (TRK-015 uses camera_id).
- Fusion strategy: nearest-neighbour gating across cameras (same logic as single-camera, but observations arrive with different timestamps and different uncertainty).
- Temporal alignment: remote frames may be delayed. Must handle cross-camera timestamp offsets.
- Redundancy: if one camera fails, the other continues tracking (graceful degradation per ADR-003).
- Blind-spot coverage: the reason for multi-camera is to cover areas invisible to a single camera.

## Acceptance

- Tracker accepts observations from multiple cameras (identified by `camera_id`).
- Each camera has its own calibration (intrinsics + homography), loaded independently.
- Observations from all cameras project into FloorPlane2D and are gated against existing tracks.
- Cross-camera observation merging: if two cameras see the same object, produce one track (not two).
- Temporal compensation: handle ≤20ms timestamp offset between cameras without double-counting.
- Graceful degradation: if one camera disconnects, remaining cameras continue providing tracks.
- `system_health` reports per-camera status (active/disconnected/degraded).

## Plan

Tier `design` — architectural decisions on fusion strategy, temporal alignment, and multi-calibration management.

### Sub-tickets (to be created when Phase 3 begins)

| Child | Title | Scope |
|-------|-------|-------|
| TRK-027a | Multi-camera observation model | Extend Observation with camera_id, per-camera uncertainty |
| TRK-027b | Multi-calibration manager | Load/manage N calibration sets, per-camera CoordinateMapper |
| TRK-027c | Cross-camera gating + fusion | Temporal alignment, duplicate suppression, track merging |
| TRK-027d | Graceful degradation on camera loss | Health monitoring per camera, fallback behaviour |

### Open design questions

1. **Fusion timing:** process all cameras synchronously (wait for all within window) or asynchronously (process each as it arrives)?
2. **Duplicate suppression:** Mahalanobis distance across cameras with combined uncertainty, or simpler spatial threshold?
3. **Track-to-camera association:** should a track record which camera(s) currently observe it?

## Log

- 2026-05-31: created. Status: backlog. Phase 3 gate. Depends on CAM-001 (remote frames) and full v0.3 pipeline.
