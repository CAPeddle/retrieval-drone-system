---
id: TRK-021
status: backlog
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-020c"
  - "TRK-002"
spec: null
plan: null
blockers: []
---

## Context

The ZMQ publisher serialises the TrackingSnapshot into a JSON message and sends it over the PUB socket per ADR-002. Every message includes `schema_version`, `message_id` (monotonic), `publish_timestamp_ms`, `frame_capture_timestamp_ms`, a `system_health` block, and an `objects` array. Socket configuration: `SNDHWM=1`, `CONFLATE=1`, `LINGER=0`. The publisher runs on the export thread, not the tracker thread — it receives snapshots via a second SPSC queue.

## Acceptance

- A `ZmqPublisher` class that serialises `TrackingSnapshot` to JSON and publishes via ZMQ PUB.
- Socket bound to configurable endpoint (default `tcp://*:5555` per ADR-002).
- Socket options set per ADR-002: `SNDHWM=1`, `CONFLATE=1`, `LINGER=0`.
- JSON schema (v0.3):
  ```json
  {
    "schema_version": 1,
    "message_id": <monotonic uint64>,
    "publish_timestamp_ms": <uint64>,
    "frame_capture_timestamp_ms": <uint64>,
    "system_health": { "tracker_state": "...", "calibration_status": "...", "cpu_temp_c": float, ... },
    "objects": [{ "object_type": "...", "track_id": int, "track_state": "...", "position": {...}, "safe_for_control": bool, "unsafe_reasons": [...] }]
  }
  ```
- `message_id` is monotonically increasing, never resets during a session.
- Serialisation cost ≤ 0.5 ms per snapshot (JSON for v0.3 debuggability).
- No heap allocation in the publish path beyond the JSON serialisation buffer (pre-sized).
- Unit tests: serialise a known snapshot → validate JSON against expected schema; verify monotonic IDs; verify socket options set correctly.

## Plan

U1. Create `src/core/export/zmq_publisher.hpp` with `ZmqPublisher` class.
U2. Implement constructor: create ZMQ context and PUB socket, set socket options per ADR-002, bind to configured endpoint.
U3. Implement `publish(const TrackingSnapshot& snapshot)`: serialise to JSON using nlohmann/json, send as ZMQ message.
U4. Implement JSON serialisation following the v0.3 schema exactly.
U5. Implement monotonic `message_id` counter (atomic uint64).
U6. Add `publish_timestamp_ms` as the time of serialisation (not frame capture time).
U7. Pre-allocate JSON serialisation buffer (estimate max message size, reserve).
U8. Add config field: `zmq.bind_address` (default `tcp://*:5555`).
U9. Unit tests: schema validation, monotonic IDs, socket option verification.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-020 (SafetyResult in snapshot) and TRK-002 (cppzmq + nlohmann/json).
