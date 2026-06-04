---
id: LASER-001
status: backlog
subsystem: laser
tier: design
created: 2026-05-31
updated: 2026-05-31
depends_on: []
spec: null
plan: null
blockers: []
---

## Context

The LaserController adapter (ADR-008) is a separate Python process that owns the serial connection to the laser MCU. In Phase 4, it becomes a consumer of the tracking core's ZMQ stream (to know where the ball is) and a commander of the MCU (to steer the laser toward the ball). It implements failsafe-off semantics: if the ZMQ stream is lost or `safe_for_control` goes false, the laser is turned off immediately.

Key constraints per ADR-008:
- **Separate process.** Never embedded in the tracking core.
- **Failsafe-off.** Default state is laser OFF. Laser ON only when explicitly commanded AND `safe_for_control = true` in the most recent snapshot.
- **Serial ownership.** Only the LaserController talks to the MCU. No other process may send serial commands.
- **Config parity.** Reads the same YAML config as the tracking core for modulation parameters (ADR-005: 15Hz, 50/50 duty). `config_hash` in system_health allows drift detection.

## Acceptance

- A Python service at `services/laser_controller/` that:
  - Subscribes to the tracking core's ZMQ PUB stream.
  - Opens serial connection to the laser MCU.
  - When `safe_for_control = true`: commands laser ON with modulation parameters.
  - When `safe_for_control = false`, heartbeat timeout, or ZMQ disconnect: commands laser OFF immediately (failsafe).
  - Reads shared YAML config for modulation parameters (frequency, duty cycle).
  - Publishes its own health status (optional secondary ZMQ PUB for operator monitoring).
- State machine: OFF → ARMED (safe_for_control true but waiting for command) → ON → OFF (failsafe).
- Latency: failsafe-off response time ≤ 50ms from `safe_for_control` flip or heartbeat timeout.
- Logging: all state transitions, serial commands sent, failsafe triggers.
- Integration test: mock ZMQ source + mock serial → verify state transitions and failsafe timing.

## Plan

Tier `design` — requires MCU serial protocol definition and state machine design.

### Sub-tickets (to be created when Phase 4 begins)

| Child | Title | Scope |
|-------|-------|-------|
| LASER-001a | Serial protocol definition | Command format, response codes, error handling |
| LASER-001b | State machine implementation | OFF/ARMED/ON/FAILSAFE transitions |
| LASER-001c | ZMQ consumer + failsafe logic | Heartbeat monitoring, safe_for_control gating |
| LASER-001d | Integration testing | Mock serial + mock ZMQ, timing assertions |

## Log

- 2026-05-31: created. Status: backlog. Phase 4 gate. Depends on v0.3 pipeline (ZMQ stream established) and LASER-002 (MCU firmware).
