---
id: TRK-023
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-003"
  - "TRK-021"
spec: null
plan: null
blockers: []
---

## Context

The system health block is published in every ZMQ message (both data and heartbeat). It aggregates: `tracker_state` (INITIALISING, RUNNING, DEGRADED, SHUTDOWN), `calibration_status` (VALID, DEGRADED, INVALID), `cpu_temp_c` (from Pi 5 thermal zone), `frames_processed_count`, `frames_rejected_count`, `frame_drop_rate`, `config_hash` (for detecting YAML drift with LaserController per ADR-008), and `thermal_throttled` flag. This enables consumers and operators to assess system health without additional monitoring infrastructure.

## Acceptance

- A `SystemHealth` struct containing all health fields.
- `tracker_state` derived from pipeline state (grace period → INITIALISING, normal → RUNNING, thermal throttle → DEGRADED).
- `calibration_status` derived from TRK-024's health monitoring (or VALID by default until that ticket lands).
- `cpu_temp_c` read from `/sys/class/thermal/thermal_zone0/temp` on Linux (Pi 5), mock on other platforms.
- `config_hash` computed at startup from YAML file content (SHA-256 truncated or CRC32).
- All fields updated by appropriate pipeline components via a shared health aggregator.
- Health aggregator uses `std::shared_mutex`: pipeline threads update (exclusive), publisher reads (shared).
- Unit tests: aggregation from multiple sources, thermal reading mock, config hash stability.

## Plan

U1. Create `src/core/system_health.hpp` with `SystemHealth` struct and `HealthAggregator` class.
U2. Implement `HealthAggregator`: thread-safe read/write with shared_mutex.
U3. Implement thermal reader: `/sys/class/thermal/thermal_zone0/temp` on Linux, fallback to 0.0 on other platforms.
U4. Implement thermal throttle detection: if `cpu_temp_c > 80`, set `thermal_throttled = true` and `tracker_state = DEGRADED`.
U5. Implement config hash: CRC32 of YAML file content at startup.
U6. Implement frame counters: `frames_processed_count`, `frames_rejected_count` updated by pipeline.
U7. Integrate: pipeline components call `health.update_*()` methods; publisher calls `health.snapshot()` for each message.
U8. Unit tests: concurrent access, thermal state transitions, counter increments.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-003 (config for hash) and TRK-021 (publisher consumes health).
