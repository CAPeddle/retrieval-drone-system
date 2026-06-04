---
id: TRK-015
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-014"
spec: null
plan: null
blockers: []
---

## Context

Observation gating determines which new detection belongs to which existing track. When a new observation arrives, it must be associated with the most likely existing track or spawn a new Provisional track. For v0.3 (single laser, single ball), the association is simple — at most one track per object type. But the gating logic must be general enough to handle the multi-object case (multiple calibration markers) and be forward-compatible with multi-camera fusion (Phase 3+).

## Acceptance

- A `Tracker` class (replacing the current stub) that maintains a set of active `Track` objects.
- `update(std::vector<Observation> observations) -> std::vector<Track>` processes all observations for one frame.
- Gating logic:
  - For each observation, compute distance to all existing tracks of the same object type.
  - If distance < `gating.max_distance_px` (configurable), associate with nearest track.
  - If no track within gate, create a new Provisional track.
  - If a track receives no observation, call `tick(false, ...)` to advance its state machine.
- Distance metric: Euclidean distance in image-pixel space (v0.3); Mahalanobis in future.
- Handles the single-object-per-type constraint for laser and ball (at most one Confirmed laser track, one Confirmed ball track).
- Unit tests: observation near existing track → associated; observation far from any track → new Provisional; track with no observation → ages; two observations for same type → nearest wins.

## Plan

U1. Create `src/core/tracking/tracker.hpp` replacing the current stub `tracker.cpp`.
U2. Implement `update()`: iterate observations, compute distances to active tracks, apply gating.
U3. Implement association: observation within gate → feed to track's `tick(true, ...)`.
U4. Implement track creation: observation outside all gates → new `Track(Provisional)`.
U5. Implement per-type constraint: if a new Confirmed track would exceed the per-type limit (1 for laser/ball, N for markers), keep the one with more observations.
U6. Call `tick(false, ...)` on all tracks that received no observation this cycle.
U7. Remove Retired tracks from active set.
U8. Add config fields: `gating.max_distance_px` (default 50).
U9. Unit tests: association, creation, aging, retirement, type constraints.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-014 (Track class with state machine).
