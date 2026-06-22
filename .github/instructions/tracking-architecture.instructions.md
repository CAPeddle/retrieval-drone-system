---
description: "Use when working on transport, subsystem boundaries, adapters, calibration/control seams, phase scope, or ZMQ message contracts for the tracking system."
---
# Tracking System Architecture

## What the Core Does (v0.3 scope)

Single Linux process on Pi 5:
1. Ingest frames from one CSI camera.
2. Detect laser dot (modulation-correlated, ADR-005) and ball.
3. Maintain per-object tracks (Provisional → Confirmed → Predicted → Occluded → Lost → Retired).
4. Project image-plane detections onto FloorPlane2D world frame (ADR-006) with per-class Z compensation (ADR-010).
5. Evaluate `safe_for_control` predicate (ADR-007) on a single snapshot.
6. Publish state over ZMQ PUB (ADR-002) + heartbeat ≥ 1 Hz even when data plane is silent.
7. Continuous calibration health check via static markers (ADR-004 Phase 2).

## What the Core Never Does

- Speaks MAVLink. Wrap in the MAVLink adapter (Phase 3+).
- Commands the laser. LaserController adapter (ADR-008) owns that.
- Commands the drone. Flight controller stack owns that.
- Auto-updates calibration without operator action (ADR-009 is Proposed, not Accepted).
- Writes to SD card on the hot path. Async ring-buffer to tmpfs only.
- Assumes a specific camera count beyond current calibration.

## Adjacent Subsystems (separate processes, contract-bound)

| Subsystem | Process | Contract |
|-----------|---------|----------|
| LaserController (ADR-008) | Python on Pi 5 | Owns MCU serial; failsafe-off. Core does not command laser. |
| MAVLink adapter (Phase 3+) | Separate process | Subscribes to ZMQ, translates to `VISION_POSITION_ESTIMATE`. |
| Viewer | Python (Pi 5 or laptop) | Read-only ZMQ SUB. |
| Replay logger | Python | ZMQ SUB → structured log for ADR-007 replay tests. |

## ZMQ Contract (ADR-002)

- Core **BINDs**; consumers **CONNECT**.
- Socket options: `SNDHWM=1`, `CONFLATE=1`, `LINGER=0`.
- Messages carry `schema_version`. Silence (no heartbeat for > 1 s) = process dead.
- Heartbeat thread is load-bearing — never let the core go silent while running.

## Calibration Lifecycle (ADR-004)

Three phases: (1) offline intrinsics, (2) runtime health via static markers, (3) active refinement (ADR-009 — deferred, Proposed only).
Route through `calibration_status`: `INVALID` / `DEGRADED` / `VALID`.

## The "Small Python Adapter" Trap

When a new external system needs integration, do NOT bolt a helper onto the core via pybind11 or shared memory. ADR-001 forbids this. Each adapter is its own process in its owning top-level subsystem directory, communicates via ZMQ, and fails independently.

## Historical Note

The v0.2-era ArUco/UDP/`solvePnP` pipeline is **superseded**. Do not extend or propose hybrids combining it with the ZMQ architecture.
