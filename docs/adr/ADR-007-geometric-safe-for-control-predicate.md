# ADR-007: Geometric `safe_for_control` Predicate

## Status
Accepted (2026-05-08)

## Context
The downstream consumer (drone flight controller via the MAVLink adapter) acts on a single boolean per published object: `safe_for_control`. The semantics of this boolean must be precise enough to be unit-tested, conservative enough to be falsifiable, and bounded enough that no consumer logic is required to second-guess it.

A loose or implicit predicate is unacceptable. If the predicate is "Confirmed track and recent observation," consumers will be tempted to add their own checks (uncertainty thresholds, alignment checks, stillness checks), each of which silently diverges from the producer's intent over time.

The latency-robust contract (locked earlier in v0.3 design) says: *the producer never lies about freshness, never lies about safety; the consumer hovers when truth degrades.* This places the burden of truth squarely on the predicate.

Forces:
- The predicate must be evaluable from a single `TrackingState` snapshot — not require historical lookback or cross-message integration.
- All input quantities must already be available in the message schema.
- `safe_for_control = true` must be a guarantee, not a hint. `safe_for_control = false` must be the default in any ambiguous condition.
- The 2 cm alignment tolerance was specified by the user as the geometric criterion that links laser intent to ball location.

## Decision
`safe_for_control` is `true` for an object only if **all** of the following clauses evaluate true. If any clause is false or undefined, `safe_for_control` is `false` and the affected reasons are listed in `unsafe_reasons`.

```
safe_for_control(snapshot) = TRUE  iff  ALL of:

  1. snapshot.system_health.tracker_state == RUNNING
  2. snapshot.system_health.calibration_status == VALID
  3. laser.track_state == Confirmed
  4. ball.track_state == Confirmed
  5. laser.age_since_last_observation_ms < AGE_MAX_MS                  (default 50)
  6. ball.age_since_last_observation_ms  < AGE_MAX_MS                  (default 50)
  7. laser.speed_m_per_s < LASER_SETTLED_SPEED_M_PER_S                  (default 0.05)
  8. distance(laser.position, ball.position)
       < ball.radius_m + ALIGNMENT_TOLERANCE_M                          (default 0.02)
```

Mappings:
- Clause 1 fails → `unsafe_reasons += ["TrackerNotRunning"]`
- Clause 2 fails → `unsafe_reasons += ["CalibrationInvalid"]`
- Clauses 3–4 fail → `unsafe_reasons += ["LaserNotConfirmed"]` / `["BallNotConfirmed"]`
- Clauses 5–6 fail → `unsafe_reasons += ["AgeExceedsThreshold"]`
- Clause 7 fails → `unsafe_reasons += ["LaserInTransit"]`
- Clause 8 fails → `unsafe_reasons += ["LaserBallMisaligned"]`

### Configuration parameters

All thresholds are static configuration in v0.3, set in `tracking_core.yaml`:
- `AGE_MAX_MS` (default 50)
- `LASER_SETTLED_SPEED_M_PER_S` (default 0.05)
- `ALIGNMENT_TOLERANCE_M` (default 0.02)
- `ball.radius_m` (no default; must be set per deployment)

### Hysteresis on flip-to-true

To prevent chatter, once `safe_for_control` flips from `true` to `false`, it must remain `false` for at least `MIN_UNSAFE_DWELL_MS` (default 200 ms) before being permitted to flip back to `true`. The flip-from-false-to-true direction is permitted to be slow; the flip-from-true-to-false direction is immediate. This asymmetry is intentional: producing a false negative (saying unsafe when the geometry is fine) is an acceptable cost; producing a false positive (saying safe when the geometry is wrong) is not.

*Clarification (2026-07-18):* the dwell is anchored at the most recent **failing evaluation**, not only at the flip-to-false itself — while `safe_for_control` is `false`, every evaluation with any failing clause re-anchors the dwell, so flipping back to `true` requires `MIN_UNSAFE_DWELL_MS` of continuously passing evaluations. Under the anchor-only-at-flip reading, clauses alternating pass/fail at frame rate would produce a one-frame `true` window every dwell period — recurring chatter this section exists to prevent. The re-anchor rule is strictly more conservative (only adds false negatives) and matches the intended "continuous safe evaluation" acceptance in TRK-020c.

### Falsifiability

The predicate is falsifiable: given a recorded scenario with ground truth (e.g., a high-speed reference camera observing both laser and ball), one can independently compute whether the predicate output was correct for each frame. ADR-007 includes a v0.3 acceptance test requirement: a replay test suite must show **zero false positives** (predicate said `true` when ground truth said the laser dot was outside the ball + 2 cm envelope) across at least three recorded scenarios totalling at least 5 minutes of footage.

## Consequences

**Positive:**
- The predicate has a single, testable definition. Consumers do not implement their own.
- Every false output carries an explicit reason, making consumer-side debugging tractable.
- Conservative defaults (false on ambiguity, asymmetric hysteresis) match the safety contract.
- The predicate is a pure function of one snapshot — easy to unit-test, easy to replay-test.

**Negative:**
- Rule 7 (laser settled) means the predicate is `false` while the laser is in motion, even if it is briefly aligned with the ball mid-sweep. This is intentional but may be surprising; documented in the design document.
- Rule 8 depends on `ball.radius_m` being correct. A misconfigured ball radius will produce systematically incorrect predicate outputs. Mitigated by surfacing `ball.radius_m` in the published `system_health` block so consumers can sanity-check.

**Risks:**
- **Uncertainty under-estimation:** rule 8 uses point-distance, not distance-with-uncertainty-margin. If the per-position `uncertainty_m` is large, the predicate may say `true` when the true position is outside the envelope. Mitigated by an additional implicit check: `laser.uncertainty_m + ball.uncertainty_m < ALIGNMENT_TOLERANCE_M`. Captured as clause 9 in implementation; included in v0.4 once empirical uncertainty calibration is complete. **For v0.3, document this as a known limitation and validate via the replay test suite.**
- **Snapshot atomicity:** the snapshot must be coherent — laser and ball positions must come from the same `TrackingState` evaluation cycle, not be combined from out-of-order messages. Mitigated by computing the predicate inside the C++ core, not in the consumer.

## Alternatives Considered
- **Consumer-side predicate.** Rejected: shifts safety semantics to every consumer independently, allowing divergence.
- **Single threshold on a composite "confidence" score.** Rejected: a single number cannot distinguish "unsafe because laser moving" from "unsafe because misaligned" — debugging would be much harder.
- **Symmetric hysteresis (slow in both directions).** Rejected: a slow flip-to-false delays the consumer's response to a real safety event.

## Related ADRs
- ADR-002 (ZeroMQ Control Transport) — every published object includes the predicate output.
- ADR-005 (Active Laser Modulation) — laser detection feeds into clause 3.
- ADR-006 (FloorPlane2D World Frame) — distance in clause 8 is computed in this frame.
- ADR-010 (Object-Class Z Compensation) — used for ball position before distance computation.
