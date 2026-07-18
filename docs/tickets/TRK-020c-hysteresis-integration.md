---
id: TRK-020c
status: done
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-020a"
  - "TRK-020b"
spec: null
plan: null
blockers:
  - "TRK-020a and TRK-020b must complete"
---

## Context

Child of TRK-020. Implements the asymmetric hysteresis mechanism and integrates the full safe_for_control evaluator into the pipeline. Hysteresis prevents chatter: once `safe_for_control` flips false, it stays false for at least `MIN_UNSAFE_DWELL_MS` (default 200 ms) before being permitted to flip back to true. The flip-to-false direction is immediate.

## Acceptance

- Full `evaluate(snapshot)` method combining clauses 1-8 from TRK-020a/020b.
- Hysteresis state: tracks the timestamp of last flip-to-false.
- Flip-to-false: immediate, no delay.
- Flip-to-true: requires all clauses passing for at least `MIN_UNSAFE_DWELL_MS` continuous evaluation cycles.
- `SafetyResult.hysteresis_remaining_ms` reports time remaining before a true→true flip is permitted.
- Evaluator integrated into pipeline: called once per frame after tracking and coordinate mapping.
- Result included in the TrackingSnapshot published over ZMQ.
- Config field: `safe_for_control.min_unsafe_dwell_ms` (default 200).
- Unit tests: rapid oscillation scenario → stays false; sustained safe → flips true after dwell; flip-to-false is instantaneous; timing precision.

## Plan

U1. Add hysteresis state to `SafeForControlEvaluator`: `last_flip_to_false_ns` (uint64), `currently_safe` (bool).
U2. Implement full `evaluate()`: call clause groups from TRK-020a/020b, then apply hysteresis logic.
U3. Hysteresis: if all clauses pass AND `now - last_flip_to_false > MIN_UNSAFE_DWELL_MS` → safe=true. Otherwise safe=false.
U4. Compute `hysteresis_remaining_ms` for reporting.
U5. Integrate into pipeline: call after tracker + coord mapper produce a snapshot.
U6. Unit tests: chatter scenario (rapid true/false oscillation), sustained-safe-after-dwell, immediate flip-to-false.
U7. Validate all TRK-020 parent acceptance criteria.

## Log

- 2026-05-31: created. Status: backlog. Final child of TRK-020. Blocked on TRK-020a and TRK-020b.
- 2026-07-18: backlog -> done. Hysteresis + integration landed. SEMANTICS NOTE (plan KTD-8): this ticket's acceptance wording (continuous-pass) and ADR-007's letter (anchor-at-flip) diverged; resolved to the conservative re-anchor-on-every-failing-evaluation rule, user-approved and recorded as the ADR-007 clarification dated 2026-07-18. Anchor arms at RUNNING entry (no startup false positive); chatter can never flip true. Evaluated once per frame in main.cpp; flip events logged.
