---
id: TRK-016
status: done
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-015"
spec: null
plan: null
blockers: []
---

## Context

Track prediction provides position estimates during frames where no observation is received (the track is in Predicted or Occluded state). For v0.3, a simple constant-velocity model is sufficient: extrapolate position from the last two observations. This keeps the track's published position fresh during brief occlusions, which matters for `safe_for_control` (clause 5/6 age check). More sophisticated prediction (Kalman filter) is deferred unless the constant-velocity model proves insufficient in replay testing.

## Acceptance

- A `predict(Track& track, uint64_t now_ns)` function or method that updates `track.position` when no observation is available.
- Prediction model: constant velocity. `position_predicted = position_last + velocity * dt`.
- Velocity estimated from the last two observations: `v = (pos_n - pos_n-1) / (t_n - t_n-1)`.
- Predicted position carries an uncertainty that grows linearly with time since last observation.
- If no velocity is available (fewer than 2 observations), position stays at last known, uncertainty grows faster.
- Prediction is capped: after `track.max_predict_duration_ms` (default 100 ms), stop predicting (track ages to Occluded/Lost via TRK-014 state machine).
- Unit tests: constant-velocity object → prediction matches; stationary object → prediction stays put; uncertainty growth over time.

## Plan

U1. Add `velocity` field to `Track` struct (float x, y in pixel space; converted to world space after coord mapping).
U2. Update velocity estimate on each new observation: `v = delta_pos / delta_time`.
U3. Implement `predict()`: compute predicted position, update uncertainty.
U4. Cap prediction duration — beyond limit, stop updating position (let state machine handle degradation).
U5. Add config field: `track.max_predict_duration_ms` (default 100).
U6. Unit tests: prediction accuracy for linear motion, stationary handling, uncertainty growth, cap behaviour.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-014 (track struct) and TRK-015 (tracker calls predict for unobserved tracks).
- 2026-07-18: backlog -> done. Constant-velocity prediction landed: observation-backed velocity with dt<=0 guards, extrapolation capped at max_predict_duration_ms, linear uncertainty growth (faster without velocity). Rates provisional pending replay footage.
