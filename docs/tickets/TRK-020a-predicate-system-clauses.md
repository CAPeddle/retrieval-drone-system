---
id: TRK-020a
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-016"
spec: null
plan: null
blockers: []
---

## Context

Child of TRK-020. Implements clauses 1-4 of ADR-007's safe_for_control predicate — the system health and track state checks.

- Clause 1: `tracker_state == RUNNING`
- Clause 2: `calibration_status == VALID`
- Clause 3: `laser.track_state == Confirmed`
- Clause 4: `ball.track_state == Confirmed`

## Acceptance

- `evaluate_system_clauses(snapshot) -> std::vector<UnsafeReason>` returns empty if all pass.
- Each failing clause appends its specific reason: `TrackerNotRunning`, `CalibrationInvalid`, `LaserNotConfirmed`, `BallNotConfirmed`.
- Unit tests: one test per clause where only that clause fails (all others pass), verify correct reason produced.
- Tests cover: INITIALISING state → clause 1 fails; DEGRADED calibration → clause 2 fails; Provisional laser → clause 3 fails; Lost ball → clause 4 fails.

## Plan

U1. Create `src/core/safety/safe_for_control.hpp` with `SafeForControlEvaluator` class, `SafetyResult` struct, `UnsafeReason` enum.
U2. Define `TrackingSnapshot` struct consumed by evaluator (or use existing pipeline output structure).
U3. Implement clause 1: check `snapshot.system_health.tracker_state == RUNNING`.
U4. Implement clause 2: check `snapshot.system_health.calibration_status == VALID`.
U5. Implement clause 3: find laser track, check `track_state == Confirmed`.
U6. Implement clause 4: find ball track, check `track_state == Confirmed`.
U7. Unit tests: individual clause failures with all-else-passing snapshots.

## Log

- 2026-05-31: created. Status: backlog. Child of TRK-020. Can proceed in parallel with TRK-020b.
