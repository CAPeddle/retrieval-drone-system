---
id: VIEW-003
status: done
subsystem: viewer
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "VIEW-002"
  - "TRK-020"
spec: null
plan: null
blockers: []
---

## Context

Extends the Python viewer (VIEW-002) with rich `safe_for_control` visualisation: shows which clauses are passing/failing in real time, displays hysteresis countdown, and provides a timeline view showing predicate state history. This makes the safety predicate's behaviour observable and debuggable during development and integration testing.

## Acceptance

- Clause-by-clause breakdown display: 8 rows, each showing clause name + pass/fail status + current value vs threshold.
- Hysteresis countdown: when `safe_for_control = false` and all clauses passing, shows time remaining before permitted flip-to-true.
- Timeline view: scrolling horizontal strip showing `safe_for_control` state (green/red) over the last 30 seconds.
- Unsafe reasons highlighted: when any clause fails, its row is highlighted red with the specific value that failed (e.g., "Clause 7: speed 0.08 > 0.05 m/s").
- Alignment visualisation: draws the tolerance circle around the ball, shows laser position relative to it.
- All values update at message rate.

## Plan

U1. Add clause-breakdown panel to viewer: parse `unsafe_reasons` array, display per-clause status.
U2. Add threshold comparison display: extract current values from tracked objects, show value vs threshold.
U3. Implement hysteresis countdown display: read `hysteresis_remaining_ms` from SafetyResult.
U4. Implement timeline strip: ring buffer of last 30s of predicate state, render as coloured horizontal bar.
U5. Implement alignment visualisation: draw `ball.radius_m + ALIGNMENT_TOLERANCE_M` circle around ball, show laser relative position.
U6. Integration test: mock messages with various clause failure combinations → verify correct display state.

## Log

- 2026-05-31: created. Status: backlog. Depends on VIEW-002 (base viewer) and TRK-020 (safety result in messages).
- 2026-07-18: backlog -> done. Safety visualisation landed: 8-row clause panel (value vs threshold from the wire thresholds block + laser_ball_distance_m), hysteresis countdown, 30s timeline strip (resets on session_id change), alignment envelope drawn in-world around the ball. Schema violations force the STALLED visual so a stale SAFE never looks current.
