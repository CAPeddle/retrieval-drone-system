---
id: TRK-022
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-021"
spec: null
plan: null
blockers: []
---

## Context

Per ADR-002, the core emits a heartbeat at ≥1 Hz independent of frame ingestion. Consumers distinguish "tracker silent because nothing detected" from "tracker process dead" by the heartbeat's presence. The heartbeat runs on its own thread, publishes on the same ZMQ socket as the data plane (using a different topic or a heartbeat-typed message), and continues regardless of pipeline state.

## Acceptance

- A `HeartbeatThread` class that publishes heartbeat messages at ≥1 Hz.
- Heartbeat message is a valid JSON message with `schema_version`, `message_type: "heartbeat"`, `message_id`, `publish_timestamp_ms`, and `system_health` block.
- Uses the same ZMQ PUB socket as the data publisher (thread-safe access via ZMQ's internal thread safety for PUB sockets, or a dedicated socket on a different port — document choice).
- Continues publishing even when: no frames are being captured, no objects are detected, the pipeline is in INITIALISING state.
- Stops only on explicit shutdown (atomic stop flag).
- Heartbeat interval configurable: `zmq.heartbeat_interval_ms` (default 500 ms → 2 Hz, well above 1 Hz minimum).
- Unit test: verify heartbeat messages arrive at expected rate; verify heartbeat continues during simulated pipeline stall.

## Plan

U1. Create `src/core/export/heartbeat_thread.hpp` with `HeartbeatThread` class.
U2. Implement heartbeat loop: sleep for configured interval, publish heartbeat JSON.
U3. Heartbeat JSON includes current `system_health` (read from shared state via shared_mutex — this is NOT hot-path, so mutex is acceptable).
U4. Thread starts at core initialisation, before pipeline enters RUNNING.
U5. Add config field: `zmq.heartbeat_interval_ms` (default 500).
U6. Document ZMQ socket sharing strategy (separate inproc socket forwarded to same PUB, or direct use — ZMQ PUB is thread-safe for sends).
U7. Unit tests: timing assertions, continuation during pipeline stall simulation.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-021 (ZMQ publisher infrastructure).
