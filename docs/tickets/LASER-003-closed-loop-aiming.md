---
id: LASER-003
status: backlog
subsystem: laser
tier: design
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "LASER-001"
  - "LASER-002"
  - "LASER-004"
spec: null
plan: null
blockers: []
---

## Context

Phase 4 closed-loop laser aiming: the LaserController subscribes to the tracking core's ZMQ stream, extracts ball position, computes the required servo/galvo angles to point the laser at the ball, and commands the MCU to move the laser. The tracking core then observes the laser dot moving toward the ball and reports the alignment — closing the feedback loop.

This is the first closed-loop control in the system. Key concerns:
- **Control rate:** limited by tracking core update rate (60 Hz) and servo/galvo response time.
- **Coordinate transform:** floor-plane position → laser mount angles (requires known laser mount position and orientation).
- **Stability:** P/PD control on the angular error. Overshoot = laser wandering off ball = `safe_for_control` goes false = laser OFF (failsafe). Must be stable.
- **Servo vs galvo:** servos are cheap/slow (~20ms response), galvos are fast/expensive (~1ms). Decision affects control bandwidth.
- **Latency:** end-to-end (ball moves → tracking core publishes → adapter computes → servo moves → tracking verifies) must be < 200ms for usable tracking.

## Acceptance

- LaserController computes aim angle from ball position and commands servo/galvo.
- Control loop: proportional or PD, stable without oscillation.
- Aim convergence: from arbitrary start position, laser lands within `ALIGNMENT_TOLERANCE_M` of ball within 2 seconds.
- Steady-state: laser tracks ball moving at ≤ 0.5 m/s with ≤ 10mm steady-state error.
- Safety: any divergence that moves laser away from ball → `safe_for_control` goes false → failsafe OFF.
- Configurable: mount position, orientation, PD gains, slew rate limits.

## Plan

Tier `design` — requires actuator decision, mount geometry calibration, and control loop tuning.

### Sub-tickets (to be created when Phase 4 begins)

| Child | Title | Scope |
|-------|-------|-------|
| LASER-003a | Actuator decision | Servo vs galvo, response time, precision analysis |
| LASER-003b | Mount geometry calibration | Laser mount position + orientation → angle-to-floor mapping |
| LASER-003c | Control loop implementation | PD controller, gain tuning, slew rate limiting |
| LASER-003d | End-to-end integration test | Ball on track → laser follows → safe_for_control validates |

### Open design questions

1. **Actuator:** hobby servos (cheap, ~50ms response, 0.5° resolution) vs galvanometer mirrors (fast, expensive, high resolution)?
2. **Mount calibration:** automated (use tracking core to observe laser at known angles) or manual (measure mount geometry)?
3. **Predictive aiming:** should the controller lead a moving ball (predict future position) or purely reactive?
4. **Multi-axis:** pan-tilt (2-axis) or single-axis on rail? Depends on physical mount design.

## Log

- 2026-05-31: created. Status: backlog. Phase 4 gate. Depends on LASER-001 (adapter), LASER-002 (MCU), and LASER-004 (servo characterisation).
