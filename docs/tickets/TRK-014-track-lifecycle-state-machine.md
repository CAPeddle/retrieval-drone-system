---
id: TRK-014
status: done
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-009d"
  - "TRK-010"
spec: null
plan: null
blockers: []
---

## Context

The track lifecycle state machine manages the lifecycle of every tracked object (laser, ball, calibration marker). States per the v0.3 design: Provisional → Confirmed → Predicted → Occluded → Lost → Retired. Each state has defined entry conditions, dwell limits, and transition triggers. The state machine is consumed by the `safe_for_control` predicate (ADR-007 clauses 3-4 require `track_state == Confirmed`).

## Acceptance

- A `Track` class with a `TrackState` enum: `Provisional`, `Confirmed`, `Predicted`, `Occluded`, `Lost`, `Retired`.
- Each track holds: `track_id` (uint32), `object_type` (enum: Laser, Ball, CalibrationMarker), `state`, `age_since_last_observation_ms`, `observation_count`, `position` (latest), `velocity` (estimated), `creation_timestamp_ns`.
- State transitions:
  - `Provisional → Confirmed`: observation_count ≥ `track.confirm_threshold` (default 3).
  - `Confirmed → Predicted`: missed one observation cycle but within age limit.
  - `Confirmed/Predicted → Occluded`: missed observations but age < `track.occlude_timeout_ms`.
  - `Occluded → Lost`: age exceeds `track.occlude_timeout_ms`.
  - `Lost → Retired`: age exceeds `track.retire_timeout_ms`. Retired tracks are removed from active set.
- No transition skips states (except direct to Retired on explicit removal).
- Unit tests: every valid transition, every invalid transition (assert no change), timer-driven transitions, multiple tracks in parallel.
- Track IDs are monotonically increasing, never reused.

## Plan

U1. Create `src/core/tracking/track.hpp` with `TrackState` enum and `Track` class.
U2. Implement state transition logic as a `tick(bool observation_received, uint64_t now_ns)` method.
U3. Implement `Provisional → Confirmed` based on observation count.
U4. Implement timer-based transitions: `Confirmed → Predicted → Occluded → Lost → Retired` based on configurable timeouts.
U5. Add config fields: `track.confirm_threshold` (default 3), `track.predict_timeout_ms` (default 50), `track.occlude_timeout_ms` (default 200), `track.retire_timeout_ms` (default 1000).
U6. Implement monotonic ID generator (atomic counter).
U7. Unit tests: full state machine coverage — every edge, timer boundaries, parallel tracks.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-009/010 (detectors produce observations to feed tracks).
- 2026-07-18: backlog -> done. TrackLifecycle landed on feat/trk-014-016-track-lifecycle: six-state Track machine, capture-clock timers with ties-decay boundaries, Provisional 2-miss deletion, cascading decay, monotonic IDs; `track` config section. Full edge/boundary unit coverage.
