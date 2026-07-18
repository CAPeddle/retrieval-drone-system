---
id: TRK-020b
status: done
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-019"
spec: null
plan: null
blockers: []
---

## Context

Child of TRK-020. Implements clauses 5-8 of ADR-007's safe_for_control predicate — the temporal freshness and geometric alignment checks.

- Clause 5: `laser.age_since_last_observation_ms < AGE_MAX_MS` (default 50)
- Clause 6: `ball.age_since_last_observation_ms < AGE_MAX_MS` (default 50)
- Clause 7: `laser.speed_m_per_s < LASER_SETTLED_SPEED_M_PER_S` (default 0.05)
- Clause 8: `distance(laser.position, ball.position) < ball.radius_m + ALIGNMENT_TOLERANCE_M` (default 0.02)

## Acceptance

- `evaluate_geometric_clauses(snapshot, config) -> std::vector<UnsafeReason>` returns empty if all pass.
- Each failing clause appends: `AgeExceedsThreshold` (clauses 5/6), `LaserInTransit` (clause 7), `LaserBallMisaligned` (clause 8).
- Clause 8 distance computed in FloorPlane2D coordinates (Z-compensated per ADR-010).
- All thresholds configurable via `Config` struct.
- Unit tests: each clause in isolation — age just over threshold → fail; speed just over → fail; distance just over → fail; exactly at boundary → test both sides.
- Edge case: ball or laser position unavailable (no Confirmed track) → handled by clauses 3/4 in TRK-020a (not reached here).

## Plan

U1. Implement clause 5: compare laser track's `age_since_last_observation_ms` against `AGE_MAX_MS`.
U2. Implement clause 6: same for ball track.
U3. Implement clause 7: compare laser track's `speed_m_per_s` against `LASER_SETTLED_SPEED_M_PER_S`.
U4. Implement clause 8: compute Euclidean distance between laser and ball FloorPlane2D positions, compare against `ball.radius_m + ALIGNMENT_TOLERANCE_M`.
U5. Accumulate reasons for any failing clause.
U6. Unit tests: individual clause failures at boundary values, combinations.

## Log

- 2026-05-31: created. Status: backlog. Child of TRK-020. Can proceed in parallel with TRK-020a.
- 2026-07-18: backlog -> done. Clauses 5-8 landed: unsafe-side-wins boundary ties (exactly-at fails), undefined speed fails clause 7, clause-8 distance on Z-compensated FloorPlane2D positions, distance exposed for the viewer.
