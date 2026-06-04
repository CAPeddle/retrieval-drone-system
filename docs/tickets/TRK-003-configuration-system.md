---
id: TRK-003
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-002"
spec: null
plan: null
blockers: []
---

## Context

Multiple ADRs reference `tracking_core.yaml` as the single source of truth for runtime configuration: modulation frequency (ADR-005), safe_for_control thresholds (ADR-007), ball radius, camera device paths, calibration file paths, ZMQ bind address. The core needs a config loader that reads YAML at startup, validates required fields, and exposes typed values to pipeline components. Configuration is NOT hot-reloaded in v0.3 — restart required for changes.

## Acceptance

- A `tracking_core.yaml` template exists with documented fields for all v0.3 parameters.
- A `Config` class/struct loads and validates the YAML at startup, returning an error on missing required fields.
- Required fields include: `camera.device_id`, `camera.target_fps`, `laser.modulation_frequency_hz`, `laser.modulation_duty_cycle`, `safe_for_control.age_max_ms`, `safe_for_control.laser_settled_speed_m_per_s`, `safe_for_control.alignment_tolerance_m`, `ball.radius_m`, `zmq.bind_address`, `calibration.intrinsics_path`, `calibration.extrinsics_path`.
- Config is immutable after construction (no runtime mutation from the hot path).
- Unit tests cover: valid config loads, missing required field errors, type mismatch errors.
- Config struct is passed by const-reference to pipeline components — no global singleton.

## Plan

U1. Create `tracking-core/config/tracking_core.yaml` template with all v0.3 fields, commented with defaults and descriptions.
U2. Create `src/core/config.hpp` declaring `struct Config` with typed members and a `static Config load(const std::string& path)` factory.
U3. Implement `config.cpp` using yaml-cpp: parse, validate required fields, populate struct.
U4. Add validation: missing fields → throw with field name; type mismatch → throw with expected vs actual.
U5. Update `main.cpp` to load config as first action, pass to pipeline components.
U6. Write unit tests: `test_config.cpp` — valid load, missing field, bad type, optional field with default.
U7. Document config fields in `tracking-core/README.md`.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-002 (yaml-cpp available).
