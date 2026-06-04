---
id: LASER-002
status: backlog
subsystem: laser
tier: design
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "LASER-001"
spec: null
plan: null
blockers: []
---

## Context

The laser MCU firmware drives the laser diode with the modulation pattern defined in ADR-005: 15 Hz, 50/50 duty cycle. The MCU receives serial commands from the LaserController adapter (LASER-001) and handles:
- Laser ON/OFF state.
- Modulation frequency and duty cycle configuration.
- Hardware safety: over-temperature shutdown, watchdog timeout (if no serial heartbeat from adapter, laser OFF).

Platform: likely Arduino Nano, ESP32, or RP2040 — decision TBD. Must be deterministic enough for 15 Hz modulation (period 66.7ms, toggle every 33.3ms).

## Acceptance

- MCU firmware that:
  - Drives laser diode ON/OFF per modulation parameters (15 Hz, 50/50 default).
  - Accepts serial commands: SET_STATE (ON/OFF), SET_MODULATION (freq_hz, duty_pct), HEARTBEAT.
  - Hardware watchdog: if no HEARTBEAT received within 500ms, force laser OFF.
  - Over-temperature protection (if MCU has temp sensor): shutdown above configured threshold.
  - Reports status on serial: current state, temperature, uptime.
- Timing accuracy: modulation jitter ≤ 1ms (well within ADR-005's correlation window).
- Power-on default: laser OFF, waiting for first command.
- Serial protocol: simple text or binary framing (design decision per LASER-001a).

## Plan

Tier `design` — requires MCU platform selection and serial protocol co-design with LASER-001.

### Sub-tickets (to be created when Phase 4 begins)

| Child | Title | Scope |
|-------|-------|-------|
| LASER-002a | MCU platform selection | Arduino Nano vs ESP32 vs RP2040, timer resolution analysis |
| LASER-002b | Modulation driver implementation | Hardware timer interrupt, 15Hz toggle |
| LASER-002c | Serial command handler | Parse commands, validate parameters, respond |
| LASER-002d | Watchdog + safety logic | Hardware watchdog, thermal shutdown |

## Log

- 2026-05-31: created. Status: backlog. Phase 4 gate. Co-designed with LASER-001 (serial protocol).
