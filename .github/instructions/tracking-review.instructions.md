---
description: "Use when reviewing tracking, detection, or control logic changes. Covers the four-domain review lens, common pitfalls, and key validation gates."
---
# Tracking System — Review Checklist

## Four-Domain "What If?" Test

For every change to tracking/detection/control logic, ask:

1. **Physical environment** — Does this survive variable lighting, reflective surfaces, floor non-planarity, children walking through the FoV, or camera bumps?
2. **Hardware constraints** — Does this fit Pi 5 CPU/thermal budget (~60% of one core at 60 fps)? Does it assume a GPU, more RAM, or faster network than available?
3. **Network/transport** — Does this assume sub-millisecond ZMQ latency? Does it handle the consumer being slower than the producer? Does it respect heartbeat semantics?
4. **State machine** — Does this correctly handle all track lifecycle transitions? Does it respect the single-snapshot evaluation rule for `safe_for_control`?

## Common Pitfalls (flag these in review)

### Z = 0 is not universal
The ball's centroid is NOT on the floor. Always apply ADR-010 Z compensation before computing floor-plane distances. Without it, a 6 cm ball projects ~3 cm off — exceeding the 2 cm alignment tolerance.

### Brightest pixel ≠ laser
ADR-005 modulates the laser at 15 Hz. The detector MUST correlate intensity over the 4-frame window. Any "fast path" that skips correlation is a correctness regression, not an optimisation.

### Pipeline depth ≠ per-stage latency
The 4-frame correlation window adds ~67 ms of inherent detection latency at 60 fps. This is deliberate pipeline depth, not a per-stage performance problem. Do not "optimise" it away without reopening ADR-005.

### ZMQ silence ≠ "tracker processing"
Silence for > 1 s = process dead. A consumer must NOT infer "dead" from a 200 ms data-plane gap — the heartbeat is the liveness signal.

### Specular ghost detections
A modulated laser reflected off a glossy surface produces a valid modulated signal. The tracker may attach the wrong observation. Flag any detector change that doesn't handle multi-observation disambiguation.

### Calibration drifts
Never assume calibration is permanently correct. Route through `calibration_status`. The correct response to drift is to surface it (DEGRADED), not silently correct it (ADR-009 is Proposed, not Accepted).

## Key Validation Gates (v0.3 ship-blocking)

- **ADR-007 replay test:** ≥ 3 scenarios, ≥ 5 minutes total, zero false positives on `safe_for_control`.
- **End-to-end latency:** median ≤ 30 ms, p99 ≤ 50 ms at 60 fps on Pi 5.
- **Frame drops:** ≤ 0.5% over 5 minutes steady state.
- **Memory:** RSS bounded and non-growing (within 5% of post-startup over 30 min).
- **Thermal degradation:** core must reduce to 30 fps and emit `DEGRADED` within 1 s of CPU temp > 80 °C.
