---
id: MAV-001
status: backlog
subsystem: mavlink
tier: design
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-028"
spec: null
plan: null
blockers: []
---

## Context

The MAVLink adapter bridges the tracking core's ZMQ stream to the drone's flight controller. It subscribes to `TrackingSnapshot` messages, extracts the drone's floor-plane position when `safe_for_control = true`, and publishes `VISION_POSITION_ESTIMATE` MAVLink messages to the drone's EKF. This is the seam between the tracking subsystem and the drone subsystem (CLAUDE.md §5).

Key constraints per architecture:
- The adapter is a **separate process** (ADR-001). Runs on the Pi 5 or a drone companion computer.
- **One-way:** tracking core → adapter → drone. Core does not know MAVLink exists.
- **Only publishes when `safe_for_control = true`** — the predicate gates actuation.
- Coordinate transform: FloorPlane2D (ADR-006, origin at Charuco) → NED frame (drone's EKF convention).
- Latency budget: adapter processing time ≤ 5ms (the tracking core's latency is separate).
- Failsafe: on tracking loss (`safe_for_control = false` or heartbeat timeout), stop sending position estimates → drone's EKF relies on other sensors → drone hovers/lands.
- MAVLink library choice: pymavlink (Python, well-documented) or MAVSDK (C++/Python, higher-level).

## Acceptance

- A Python service at `services/mavlink_adapter/` that:
  - Subscribes to the tracking core's ZMQ PUB stream.
  - Monitors heartbeat messages (timeout → failsafe).
  - When `safe_for_control = true`: extracts drone position, transforms to NED, publishes `VISION_POSITION_ESTIMATE`.
  - When `safe_for_control = false` or heartbeat timeout: stops publishing (failsafe-off semantics per ADR-008 pattern).
- Coordinate transform: FloorPlane2D → NED configurable (requires known relationship between floor frame origin and drone's NED origin).
- Rate control: publishes at configurable rate (default: match tracking rate, cap at 50 Hz).
- Logging: logs all state transitions (publishing/failsafe) and position estimates for post-flight analysis.
- Integration test: mock ZMQ source → adapter → captured MAVLink messages validate correct transform and gating.

## Plan

Tier `design` — requires decisions on MAVLink library, NED transform configuration, and deployment location.

### Sub-tickets (to be created when Phase 3 integration begins)

| Child | Title | Scope |
|-------|-------|-------|
| MAV-001a | MAVLink library selection | pymavlink vs MAVSDK, trade-off for Python adapter |
| MAV-001b | FloorPlane2D → NED transform | Coordinate transform definition + configuration |
| MAV-001c | Adapter implementation | ZMQ subscriber + MAVLink publisher + failsafe logic |
| MAV-001d | Integration testing | Mock source → adapter → MAVLink message validation |

### Open design questions

1. **Library:** pymavlink (low-level, well-understood) vs MAVSDK-Python (higher-level, hides protocol details)?
2. **Deployment:** same Pi 5 as tracking core, or separate companion on the drone?
3. **NED origin:** how is the drone's NED origin related to the floor frame? Is it set at arm time? Configurable offset?
4. **EKF fusion confidence:** does the adapter set the `VISION_POSITION_ESTIMATE` covariance fields? (Yes, from tracking uncertainty.)

## Log

- 2026-05-31: created. Status: backlog. Phase 3 gate. Depends on TRK-028 (drone is a tracked object) and full v0.3 pipeline.
