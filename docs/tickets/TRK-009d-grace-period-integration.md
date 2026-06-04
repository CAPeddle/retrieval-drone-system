---
id: TRK-009d
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-009c"
spec: null
plan: null
blockers:
  - "TRK-009b and TRK-009c must complete"
---

## Context

Child of TRK-009. Implements the startup grace period (ADR-005: 2 full modulation cycles = ~134 ms before any detections emitted) and integrates the complete modulation detector into the pipeline's frame-processing loop. Runs the parent ticket's full acceptance validation.

## Acceptance

- Grace period: first `2 × (fps / modulation_freq)` frames (default: 8 frames at 60fps/15Hz) produce no detections regardless of input.
- Grace period state reflected in `system_health.tracker_state = INITIALISING` until complete.
- Detector integrated into pipeline: consumer thread calls `push_frame()` then `detect()` after quality assessment passes.
- End-to-end test: synthetic modulated sequence → correct detection after grace period, no detection during grace period.
- Performance test: full detect cycle (push + correlate + cluster + centroid) ≤ 4 ms on Pi 5.
- All parent TRK-009 acceptance criteria validated.
- Config field: `laser.grace_period_cycles` (default 2).

## Plan

U1. Add frame counter to `ModulationDetector`; suppress `detect()` output until counter exceeds `grace_period_cycles × frames_per_cycle`.
U2. Surface grace period state via a `is_initialising() -> bool` method for system health to read.
U3. Integrate detector into the pipeline's frame-processing loop (after frame quality assessment, before tracking).
U4. End-to-end unit test: full synthetic sequence through quality → detect → output.
U5. Performance benchmark: time the full `detect()` path on Pi 5 representative frames, assert ≤ 4 ms.
U6. Validate all TRK-009 parent acceptance criteria; update parent Log on completion.

## Log

- 2026-05-31: created. Status: backlog. Final child of TRK-009; integrates everything.
