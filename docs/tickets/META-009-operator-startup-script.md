---
id: META-009
status: backlog
subsystem: meta
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-002"
  - "TRK-021"
spec: null
plan: null
blockers: []
---

## Context

After install, the operator needs a single command (or minimal script) to start the system: launch the tracking core, optionally start the viewer, and report readiness. Currently there's no start script — the operator must manually run cmake build output, configure endpoints, and launch components in the correct order.

## Acceptance

- A script at `tools/start.sh` (bash, runs on Pi 5) that:
  1. Checks prerequisites: camera accessible, config YAML present, calibration files present.
  2. Launches the tracking core binary with the correct config path.
  3. Waits for the heartbeat on the ZMQ port (confirms core is alive).
  4. Optionally launches the Python viewer (if `--viewer` flag).
  5. Reports startup status to stdout: "Tracking core running on tcp://*:5555, heartbeat confirmed."
  6. On failure: prints specific error (camera not found, calibration missing, etc.) and exits non-zero.
- Graceful shutdown: Ctrl+C sends SIGTERM to tracking core, waits for clean exit.
- Configurable: `TRACKING_CONFIG` env var overrides default config path.
- No systemd/service integration for v0.3 (single-operator hobby project); just a launch script.

## Plan

U1. Create `tools/start.sh` with prerequisite checks.
U2. Implement camera accessibility check (test V4L2 device exists).
U3. Implement config/calibration file existence checks.
U4. Launch tracking core as background process, capture PID.
U5. Wait for heartbeat on ZMQ port (simple Python one-liner with pyzmq, or `timeout` + `zmq_recv`).
U6. Optional viewer launch with `--viewer` flag.
U7. Implement SIGTERM handler: forward to child processes, wait for exit.
U8. Add `--help` usage documentation.

## Log

- 2026-05-31: created. Status: backlog. Depends on tracking core binary being buildable (TRK-002+) and ZMQ publisher (TRK-021).
