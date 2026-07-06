---
name: tracking-review
description: Use when reviewing any tracking, detection, control, or design change for holes — "review this", "what's wrong with", "look at this", or a code/design upload (including ADRs, tickets, and design docs). Applies the four-domain What-if test and the project pitfall checklist.
---

# Tracking Review

Review mode: hunt for holes. Be specific — "this assumes lighting is constant" is
useful; "this might have edge cases" is not. List vulnerabilities with severity and
a proposed mitigation.

## The four-domain "What if?" test (§9.4)

For any tracking change, probe all four domains:

1. **Physical environment.** What if the lighting changes? A reflective surface
   enters the laser path? The floor isn't planar here? A child walks through the
   FoV or moves the camera/markers? Components were moved between sessions?
2. **Hardware constraints.** What if the Pi 5 is thermally throttling? Co-tenant
   CPU (drone stack) is contending? The rolling shutter interacts with the 15 Hz
   modulation near frame edges? The camera focus ring was nudged?
3. **Network / transport.** What if a consumer lags (HWM=1 drops stale frames)?
   Network latency spikes? A consumer mistakes a data-plane gap for "process
   dead" instead of watching the heartbeat?
4. **State machine.** What if a track is Provisional and never confirms? An
   observation arrives during Occluded? Two observations fall within gating
   distance? Calibration flips to DEGRADED mid-run?

Also trace boundary conditions: empty frames, a single observation, two
observations within gating distance, observations outside the calibrated region.

## Pitfall checklist (§10)

Check the change against these known failure modes (full detail in
`docs/agent-reference/agent-reference.md`, "Common pitfalls"):

- **Z=0 is not universal.** Ball/drone/foot centroids are not on the floor —
  apply ADR-010 Z compensation before floor-plane distances. (~3 cm error on a
  6 cm ball exceeds the 2 cm tolerance.)
- **The brightest pixel is not the laser.** ADR-005 modulates at 15 Hz for a
  reason — sunlight, LEDs, specular reflections, a second pointer all produce
  bright pixels. A "fast path" bypassing the correlation window is a correctness
  regression, not an optimisation.
- **Detection latency is pipeline depth, not per-stage time.** The 4-frame
  correlation window is ~67 ms of inherent latency by design; don't "optimise" it
  away without instruction to reopen ADR-005. Consumers hold position when data
  is stale.
- **ZMQ silence is not a signal.** The core emits a ≥1 Hz heartbeat even when the
  data plane is silent. Silence means "process dead," never "no objects."
- **The "small Python adapter" trap.** Every external system is a separate process
  over ZMQ — no pybind11/shared-memory bolt-ons (ADR-001).
- **Calibration is not a one-time event (ADR-004).** Surface drift via
  `calibration_status` (INVALID/DEGRADED/VALID); do not silently self-correct
  (that path is ADR-009, Proposed).
- **Reproposing decided options.** When the user has said "go with X," don't
  relist rejected alternatives or hedge.

## Output

For each finding: the domain, the specific failure condition, severity, and a
concrete mitigation. Prefer falsifiable statements with named thresholds.
