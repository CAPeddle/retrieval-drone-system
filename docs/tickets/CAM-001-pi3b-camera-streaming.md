---
id: CAM-001
status: backlog
subsystem: camera
tier: design
created: 2026-05-31
updated: 2026-05-31
depends_on: []
spec: null
plan: null
blockers: []
---

## Context

Phase 3 introduces a Pi 3B as a dedicated camera streaming node. The Pi 3B captures frames from its CSI camera and streams them to the Pi 5 over the LAN. The Pi 5's tracking core ingests these frames alongside its local camera for multi-camera fusion. This ticket covers the Pi 3B streaming infrastructure — the actual fusion logic is TRK-027.

Key constraints:
- Pi 3B: limited compute (Cortex-A53, 1GB RAM). Must only capture + stream, no CV processing.
- Network: home Wi-Fi or wired LAN. Variable latency. Must tolerate packet loss gracefully.
- Transport: ZMQ PUB/SUB (consistent with ADR-002) or raw TCP/UDP — decision required.
- Frame format: raw or compressed? Bandwidth vs latency trade-off.
- Timestamping: frames must carry capture timestamps synchronised to a common time base (NTP or PTP).
- CSI cable reach (15cm) prevents multi-camera on a single Pi 5 — this is why the Pi 3B exists.
- Power: the Pi 3B under-volts on a marginal supply once the camera draws current. Spec a **5 V/2.5 A+** PSU and a short, thick cable. See [`docs/solutions/hardware/pi3b-camera-node-undervoltage.md`](../solutions/hardware/pi3b-camera-node-undervoltage.md).

## Acceptance

- Pi 3B runs a lightweight streaming daemon that captures frames and sends them to the Pi 5.
- Frames arrive at the Pi 5 with embedded capture timestamps.
- Frame delivery latency (capture → Pi 5 receipt): target < 20 ms on wired LAN.
- Frame loss handling: consumer tolerates dropped frames without crash or stall.
- Configurable resolution and frame rate (matching the Pi 5's local camera settings).
- Time synchronisation mechanism documented and validated (NTP or PTP with measured offset).
- The Pi 5 tracking core can distinguish local-camera frames from remote-camera frames (camera_id metadata).
- Power supply verified adequate under camera load — `tools/pi-power-check.sh` reports healthy (`get_throttled=0x0`, no under-voltage while streaming).

## Plan

Tier `design` — requires decisions on transport, compression, and time sync.

### Sub-tickets (to be created when Phase 3 begins)

| Child | Title | Scope |
|-------|-------|-------|
| CAM-001a | Transport + compression decision | ZMQ vs raw UDP, MJPEG vs raw, bandwidth analysis |
| CAM-001b | Pi 3B capture daemon | Lightweight V4L2 capture + network publish |
| CAM-001c | Pi 5 remote frame receiver | Ingest remote frames into pipeline with correct timestamps |
| CAM-001d | Time synchronisation validation | NTP/PTP offset characterisation, timestamp correction |

### Open design questions

1. **Transport:** ZMQ PUB/SUB (consistent with rest of system) vs raw UDP (lower overhead on Pi 3B)?
2. **Compression:** MJPEG (hardware-assisted on Pi 3B?) vs raw (simpler, higher bandwidth)?
3. **Time sync:** NTP (simpler, ~1ms accuracy) vs PTP (sub-ms but more complex)?
4. **Multi-camera calibration:** per-camera extrinsics to shared floor frame — how to handle inter-camera overlap?

## Log

- 2026-05-31: created. Status: backlog. Phase 3 gate. Depends on v0.3 being complete (single-camera pipeline proven).
