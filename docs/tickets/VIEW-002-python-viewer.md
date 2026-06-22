---
id: VIEW-002
status: backlog
subsystem: viewer
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

The Python viewer subscribes to the tracking core's ZMQ stream and displays tracked objects in the FloorPlane2D coordinate frame. It is a read-only consumer (does not command anything). For v0.3: displays laser position, ball position, and their tracks on a 2D floor-plane visualisation. Uses pygame or matplotlib for rendering (simple, no framework). Validates the ZMQ schema at runtime using pydantic.

## Acceptance

- A Python viewer script at the location established by `VIEW-001`: currently `tracking-core/src/viewer/viewer.py`, target `viewer/` after promotion.
- Connects to configurable ZMQ endpoint (default `tcp://localhost:5555`).
- Sets subscriber socket options per ADR-002: `RCVHWM=1`, `CONFLATE=1`.
- Validates incoming JSON against the v0.3 schema using pydantic (schema_version check, field validation).
- Displays:
  - Floor-plane grid (configurable scale, default 1m grid lines).
  - Laser position as a coloured dot with uncertainty circle.
  - Ball position as a circle (scaled by `ball.radius_m`) with uncertainty ring.
  - Track trails (last N positions as fading trail).
  - `safe_for_control` state prominently (green/red indicator).
  - System health summary (tracker_state, calibration_status, fps).
- Updates at received message rate (up to 60 Hz display refresh).
- Graceful handling of ZMQ disconnection (displays "DISCONNECTED" state, auto-reconnects).
- Heartbeat monitoring: if no message received within 2× heartbeat interval, displays "TRACKER DEAD" warning.

## Plan

U1. Create pydantic models for the v0.3 ZMQ message schema (`TrackingMessage`, `SystemHealth`, `TrackedObject`).
U2. Implement ZMQ subscriber with `RCVHWM=1`, `CONFLATE=1`, non-blocking receive with timeout.
U3. Implement floor-plane renderer (pygame or matplotlib): coordinate transform from metres to screen pixels.
U4. Implement object rendering: laser dot, ball circle, uncertainty rings, position trails.
U5. Implement `safe_for_control` indicator: prominent green/red with `unsafe_reasons` text overlay.
U6. Implement system health display: tracker_state, calibration_status, FPS counter.
U7. Implement connection state management: CONNECTED, DISCONNECTED, TRACKER_DEAD states.
U8. Add configuration: `viewer.zmq_endpoint`, `viewer.floor_scale_m`, `viewer.trail_length`.
U9. Integration test: mock ZMQ publisher → viewer receives and validates schema.

## Log

- 2026-05-31: created. Status: backlog. Depends on TRK-021 (ZMQ schema defined) but can start once schema is finalised.
