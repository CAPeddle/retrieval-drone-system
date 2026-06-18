# ADR-008: LaserController Adapter

## Status
Accepted (2026-05-08)

## Context
ADR-005 establishes that the laser must be modulated at a known frequency, and the modulation pattern is configured in the tracking core's YAML but physically driven by a separate microcontroller (per the user's preference to keep hard-real-time GPIO duties off the Pi 5). This requires a defined boundary between the Pi 5 (which holds the source of truth for the modulation parameters) and the laser MCU (which physically toggles the laser).

The boundary must be:
- **Robust to Pi 5 process restart** without leaving the laser in an unsafe or unknown state.
- **Robust to MCU restart or USB disconnection** without poisoning the tracking detector with an unexpected modulation pattern.
- **Failsafe by default:** any communication anomaly results in the laser being off, not in a runaway-on or stuck-pattern state.

This is also the mechanism through which the tracking system will, in later phases, command the laser to specific aim points (active calibration in ADR-009; closed-loop laser aiming in Phase 4+).

## Decision
A new component, **LaserController Adapter**, runs as a separate process on the Pi 5. It owns the serial connection to the laser MCU and acts as the broker between the tracking core's configuration and the MCU's physical actuation.

### Architecture
```
[tracking_core (C++)]              [LaserController Adapter (Python)]
        ↓                                       ↓
   reads YAML for                        owns /dev/ttyUSB0 or
   modulation pattern                    /dev/ttyACM0 to laser MCU
        ↓                                       ↓
   (no direct hardware access)           sends serial frames
                                                ↓
                                         [Laser MCU (ESP32 / Arduino)]
                                                ↓
                                         drives laser GPIO at configured pattern
```

The tracking core does not communicate with the LaserController Adapter at runtime in v0.3. Both processes read the same `tracking_core.yaml` file at startup, ensuring they have identical modulation parameters. This avoids a runtime dependency between the tracking pipeline and the laser controller.

In Phase 4+, when closed-loop laser aiming is added, the LaserController Adapter will subscribe to the tracking core's ZeroMQ stream as a consumer (per ADR-002). The serial protocol below is forward-compatible with that addition.

### Serial protocol (v0.3)

Frame format (text-based, line-delimited for v0.3 debuggability; migrate to a binary protocol in Phase 4 if latency requires):

| Frame | Direction | Payload | Purpose |
|---|---|---|---|
| `INIT freq=15.0 duty=0.5` | Pi → MCU | mod params | configure modulation at startup |
| `START` | Pi → MCU | (none) | begin modulating |
| `STOP` | Pi → MCU | (none) | turn laser off (failsafe) |
| `PING` | Pi → MCU | (none) | heartbeat (every 1 s) |
| `PONG` | MCU → Pi | (none) | ack heartbeat |
| `STATUS state=ON freq=15.0` | MCU → Pi | (state) | response to PING; periodic |

### Failsafe behaviour

The MCU implements a strict watchdog:
- If no `PING` is received within 3 s, the MCU stops modulating (laser off).
- If a malformed frame is received, the MCU logs it and remains in its previous state.
- On MCU power-up, the laser is off until an `INIT` + `START` sequence is received.

The LaserController Adapter implements a complementary watchdog:
- If no `PONG` is received within 3 PING cycles (3 s), the adapter logs an error and the laser is presumed off.
- The adapter does not retry indefinitely on persistent failure — after 30 s it surfaces a hard error to the operator (log file or system journal).

### Configuration source of truth

The single source of truth for the modulation parameters is `tracking_core.yaml`:
```yaml
laser:
  modulation_frequency_hz: 15.0
  modulation_duty_cycle: 0.5
  laser_type: infrared       # informational; for the operator
```

The LaserController Adapter and the tracking core both read this file at startup. The adapter transmits the relevant fields to the MCU in the `INIT` frame.

## Consequences

**Positive:**
- The Pi 5 has no hard-real-time GPIO obligations for laser modulation. The CPU can be fully consumed by tracking without affecting laser stability.
- The MCU is replaceable (any platform that can do PWM and serial — ESP32, Arduino, RP2040 — works) without changing the tracking core.
- Failsafe-by-default: any system failure mode results in the laser being off.
- The adapter is a small process (estimated < 200 lines of Python) and does not block the tracking pipeline.

**Negative:**
- Three components (core + adapter + MCU) must be kept consistent for the laser to function. Mitigated by the shared YAML source of truth and the operator startup script that launches both Pi 5 processes.
- A serial cable is a physical dependency. USB disconnection during operation results in laser-off (acceptable per the failsafe contract) but interrupts ADR-009 active calibration sweeps.

**Risks:**
- **Serial port enumeration race:** if multiple USB devices are plugged in, the adapter may bind to the wrong port. Mitigated by binding by USB serial number rather than `/dev/ttyUSB*` ordering. Adapter configuration includes `laser_mcu_serial: "<id>"`.
- **YAML drift between core and adapter:** if the two read different versions of the YAML (e.g., adapter restarted after a config change while core still has old config in memory), modulation pattern and detector pattern will mismatch. Mitigated by including a `config_hash` in the published `system_health` block and in the adapter's serial frames; consumers can detect mismatch.

## Alternatives Considered
- **Pi 5 drives laser GPIO directly via `pigpio` or `WiringPi`.** Rejected: the Pi 5 is already running 60–90 fps tracking; introducing hard-real-time GPIO handling on the same CPU adds latency variance. Acceptable on a less-loaded system.
- **MCU receives modulation parameters at runtime over ZeroMQ via a small bridge.** Rejected for v0.3: adds runtime coupling between tracking core and laser hardware. Acceptable as a Phase 4+ refinement.
- **Hard-coded modulation parameters in MCU firmware.** Rejected: any change to the modulation pattern (for testing alternative frequencies, for ADR-009's calibration sweeps) requires reflashing the MCU.

## Related ADRs
- ADR-005 (Active Laser Modulation) — defines the modulation parameters this adapter transmits.
- ADR-002 (ZeroMQ Control Transport) — the channel for Phase 4+ closed-loop aim control.
- ADR-009 (Active Calibration Refinement) — extends this adapter with a "drive to angle" command.
