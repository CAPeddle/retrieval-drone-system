---
id: TRK-020
status: done
subsystem: tracking-core
tier: design
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-016"
  - "TRK-019"
spec: null
plan: null
blockers: []
---

## Context

The `safe_for_control` predicate (ADR-007) is the most safety-critical component in the tracking core. It evaluates 8 clauses on a single TrackingState snapshot and produces a boolean + reasons vector. It includes asymmetric hysteresis (fast flip-to-false, slow flip-to-true). This is tier `design` because incorrect implementation has direct safety implications — the replay test suite (TRK-025) is specifically designed to validate this component.

Key ADR-007 requirements:
- 8 clauses, all must be true for `safe_for_control = true`.
- Each false clause produces a named reason in `unsafe_reasons`.
- Asymmetric hysteresis: `MIN_UNSAFE_DWELL_MS` (default 200 ms) before flip back to true.
- Pure function of one snapshot — no cross-snapshot lookback.
- Distance computation in FloorPlane2D frame using Z-compensated positions (ADR-010).

## Acceptance

- A `SafeForControlEvaluator` class with `evaluate(const TrackingSnapshot& snapshot) -> SafetyResult`.
- `SafetyResult` struct: `safe` (bool), `unsafe_reasons` (vector of enum values), `hysteresis_remaining_ms` (uint32).
- All 8 clauses implemented per ADR-007 specification.
- Hysteresis: flip-to-false is immediate; flip-to-true requires `MIN_UNSAFE_DWELL_MS` continuous safe evaluation.
- Each clause independently testable — one test per clause where that clause fails while all others pass.
- Configuration: `AGE_MAX_MS`, `LASER_SETTLED_SPEED_M_PER_S`, `ALIGNMENT_TOLERANCE_M`, `ball.radius_m`, `MIN_UNSAFE_DWELL_MS` all from config.
- Distance uses Z-compensated FloorPlane2D positions from CoordinateMapper.
- No heap allocation in `evaluate()` — `unsafe_reasons` vector pre-allocated with `reserve(8)`.
- Comprehensive unit tests: every clause individually, combinations, hysteresis timing, edge cases (exactly at threshold).

## Plan

Tier `design` — spawns sub-tickets for rigorous clause-by-clause implementation and validation.

### Sub-tickets

| Child | Title | Scope |
|-------|-------|-------|
| TRK-020a | Predicate clauses 1-4 (system + track state) | System health checks + track state checks |
| TRK-020b | Predicate clauses 5-8 (temporal + geometric) | Age, speed, alignment checks |
| TRK-020c | Hysteresis + integration | Asymmetric hysteresis, full evaluation, pipeline integration |

### Orchestration notes for overseer agent

- TRK-020a and TRK-020b can proceed in parallel (independent clause groups).
- TRK-020c depends on both TRK-020a and TRK-020b.
- Parent acceptance validated only after TRK-020c completes with full integration tests.

## Log

- 2026-05-31: created. Status: backlog. Tier `design` due to safety criticality. Depends on TRK-018/019 (coordinate mapping for distance computation) and TRK-014/015 (track state for clauses 3-4).
- 2026-07-18: backlog -> done. Parent acceptance met via TRK-020a/b/c: 8 clauses, asymmetric hysteresis (re-anchor semantics per ADR-007 clarification 2026-07-18), no-alloc evaluate, safe_for_control.min_unsafe_dwell_ms config. Verified by clause/boundary/chatter unit tests + U12 synchronous replay harness with True-Positive Counterweight; on-Pi replay gate zero safe=true frames (interim evidence pending the >=5min ball-in-scene library).
