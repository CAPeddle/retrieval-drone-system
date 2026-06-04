---
id: LASER-004
status: backlog
subsystem: laser
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "LASER-002"
spec: null
plan: null
blockers: []
---

## Context

Before closed-loop aiming (LASER-003) can be tuned, the servo/galvo accuracy must be empirically characterised. This ticket produces the measurements that gate LASER-003 and inform ADR-009's observability validation. Measurements needed:
- Angular repeatability: command same angle 100 times, measure spread.
- Angular resolution: minimum commandable step vs actual achieved step.
- Step response time: command a step, measure time to settle within 1% of final value.
- Backlash: command forward then reverse, measure hysteresis.
- Thermal drift: measure position stability over 30 minutes of continuous operation.

This is also one of two gates for promoting ADR-009 from Proposed (the other is TRK-029 observability validation).

## Acceptance

- A test rig (laser + servo/galvo + camera observation) captures measurements.
- Report documenting: repeatability (σ in degrees), resolution (minimum step in degrees), step response (ms to settle), backlash (degrees), thermal drift (degrees/hour).
- Results determine whether the chosen actuator is suitable for closed-loop control within ALIGNMENT_TOLERANCE_M at the expected working distances.
- Pass criteria: repeatability σ < 0.1° (corresponds to ~1mm at 0.5m working distance), step response < 50ms, backlash < 0.2°.
- Measurement automation: Python script commands servo positions, captures camera frames, computes laser dot positions.

## Plan

U1. Build test rig: laser + actuator + fixed camera observing a flat surface at known distance.
U2. Write Python measurement script: command servo to target angles, capture frames, detect laser dot position.
U3. Repeatability test: command same angle 100× times, compute σ of observed positions.
U4. Resolution test: sweep through minimum steps, detect smallest observable movement.
U5. Step response test: command large step, capture frames at high rate, measure settling time.
U6. Backlash test: sweep forward then reverse, measure hysteresis loop width.
U7. Thermal drift test: hold steady for 30 minutes, capture positions at 1 Hz, compute drift rate.
U8. Generate JSON report with all measurements; document pass/fail against criteria.

## Log

- 2026-05-31: created. Status: backlog. Phase 4 gate. Must complete before LASER-003 (control loop tuning) and gates ADR-009 promotion.
