# Drone System — Board

Last updated: 2026-06-04

Workflow defined in [README.md → Workflow](README.md#workflow). Risk register (R-NN / D-NN items) lives in [CLAUDE.md §13](CLAUDE.md).

## Next


## In Progress
- [ ] TRK-004 — Async logger → [story](docs/tickets/TRK-004-async-logger.md)


## Blocked


## Done (recent)

- [x] META-006 — Build board automation scripts (2026-05-31)
- [x] META-007 — Plant board-script discovery pointers and create AGENTS.md (2026-05-31)
- [x] TRK-001 — Rename `tracking-system/` to `tracking-core/` (2026-06-01)
- [x] META-010 — ADR and solution doc scaffolding scripts (2026-06-05)
- [x] META-011 — OpenCode help and agents discovery commands (2026-06-05)
- [x] META-012 — Ticket-archive command wrapper (2026-06-05)
- [x] META-013 — Explicit bash allow-list in opencode.json (2026-06-05)
- [x] DOCS-001 — Move ADRs from `Claude Synthesised/` to `docs/adr/` (2026-06-18)
- [x] DOCS-002 — Move root brainstorm `.md` files to `docs/research/` (2026-06-18)
- [x] DOCS-006 — Reconcile [CLAUDE.md §8.7](CLAUDE.md) layout with chosen multi-subsystem structure (2026-06-21)
- [x] TRK-002 — Build system overhaul (2026-07-08)
- [x] TRK-003 — Configuration system (2026-07-08)
- [x] CAM-002 — PoC: Pi3B to Pi5 camera streaming with web viewer (2026-07-15)

## Backlog

### Cleanup

- [ ] DOCS-003 — Remove or archive `Claude Synthesised.zip` from working tree
- [ ] DOCS-004 — Write missing `docs/design/v0.3-tracking-visualisation-system-design.md` (CLAUDE.md references it but file does not exist)
- [ ] DOCS-005 — Broader Copilot instruction modernisation
- [ ] VIEW-001 — Promote `viewer/` from `tracking-core/src/viewer/` to top-level dir

### Open decisions (groom before scheduling)

- [ ] META-002 — Choose replay test framework (currently in CLAUDE.md "NOT locked" list; resolves [D-05](CLAUDE.md))
- [ ] META-003 — Choose C++ library set for ZMQ, JSON, config, logging (currently in CLAUDE.md "NOT locked" list)

### Borrowed from external workflow review (2026-05-31)

- [ ] META-004 — Adopt U-IDs in ticket story plans → [story](docs/tickets/META-004-adopt-u-ids-in-plans.md)
- [ ] META-005 — Adopt Parallel Safety Check protocol for subagent dispatch → [story](docs/tickets/META-005-adopt-parallel-safety-check.md)

### v0.3 implementation — infrastructure

- [ ] TRK-005 — Frame ring buffer (lock-free SPSC) → [story](docs/tickets/TRK-005-frame-ring-buffer.md)
- [ ] TRK-006 — Camera capture thread (V4L2 + SCHED_FIFO) → [story](docs/tickets/TRK-006-camera-capture-thread.md)
- [ ] TRK-007 — Frame timestamping (FrameMetadata POD) → [story](docs/tickets/TRK-007-frame-timestamping.md)
- [ ] TRK-008 — Frame quality assessment (exposure + blur gate) → [story](docs/tickets/TRK-008-frame-quality-assessment.md)

### v0.3 implementation — detection

- [ ] TRK-009 — Modulation laser detector (design parent) → [story](docs/tickets/TRK-009-modulation-laser-detector.md)
- [ ] TRK-009a — PSD strategy benchmark → [story](docs/tickets/TRK-009a-psd-strategy-benchmark.md)
- [ ] TRK-009b — Rolling buffer + PSD correlation engine → [story](docs/tickets/TRK-009b-rolling-buffer-correlation.md)
- [ ] TRK-009c — Clustering + centroid extraction → [story](docs/tickets/TRK-009c-clustering-centroid.md)
- [ ] TRK-009d — Grace period + pipeline integration → [story](docs/tickets/TRK-009d-grace-period-integration.md)
- [ ] TRK-010 — Ball detector (IR blob + size gate) → [story](docs/tickets/TRK-010-ball-detector.md)
- [ ] TRK-011 — Calibration marker detector (ArUco/Charuco) → [story](docs/tickets/TRK-011-calibration-marker-detector.md)

### v0.3 implementation — calibration tooling

- [ ] TRK-012 — Intrinsic calibration tool (Python) → [story](docs/tickets/TRK-012-intrinsic-calibration-tool.md)
- [ ] TRK-013 — Extrinsic calibration tool (Python) → [story](docs/tickets/TRK-013-extrinsic-calibration-tool.md)

### v0.3 implementation — tracking

- [ ] TRK-014 — Track lifecycle state machine → [story](docs/tickets/TRK-014-track-lifecycle-state-machine.md)
- [ ] TRK-015 — Observation gating + track association → [story](docs/tickets/TRK-015-observation-gating.md)
- [ ] TRK-016 — Track prediction (constant-velocity) → [story](docs/tickets/TRK-016-track-prediction.md)

### v0.3 implementation — coordinate mapping

- [ ] TRK-017 — Homography loader (JSON → matrix) → [story](docs/tickets/TRK-017-homography-loader.md)
- [ ] TRK-018 — Z compensation + CoordinateMapper → [story](docs/tickets/TRK-018-z-compensation.md)
- [ ] TRK-019 — Uncertainty propagation (Jacobian-based) → [story](docs/tickets/TRK-019-uncertainty-propagation.md)

### v0.3 implementation — safety predicate

- [ ] TRK-020 — safe_for_control predicate (design parent) → [story](docs/tickets/TRK-020-safe-for-control-predicate.md)
- [ ] TRK-020a — Predicate system clauses (1–4) → [story](docs/tickets/TRK-020a-predicate-system-clauses.md)
- [ ] TRK-020b — Predicate geometric clauses (5–8) → [story](docs/tickets/TRK-020b-predicate-geometric-clauses.md)
- [ ] TRK-020c — Hysteresis + integration → [story](docs/tickets/TRK-020c-hysteresis-integration.md)

### v0.3 implementation — export + health

- [ ] TRK-021 — ZMQ publisher (JSON schema v1) → [story](docs/tickets/TRK-021-zmq-publisher.md)
- [ ] TRK-022 — Heartbeat thread (≥1 Hz) → [story](docs/tickets/TRK-022-heartbeat-thread.md)
- [ ] TRK-023 — System health reporting → [story](docs/tickets/TRK-023-system-health-reporting.md)
- [ ] TRK-024 — Calibration health monitoring (ADR-004 Phase 2) → [story](docs/tickets/TRK-024-calibration-health-monitoring.md)

### v0.3 implementation — validation + viewer

- [ ] TRK-025 — Replay test harness (design parent, resolves D-05) → [story](docs/tickets/TRK-025-replay-test-harness.md)
- [ ] TRK-026 — Performance benchmark suite → [story](docs/tickets/TRK-026-performance-benchmarks.md)
- [ ] VIEW-002 — Python ZMQ viewer (floor-plane display) → [story](docs/tickets/VIEW-002-python-viewer.md)
- [ ] VIEW-003 — safe_for_control visualisation overlay → [story](docs/tickets/VIEW-003-safe-for-control-visualisation.md)

### Phase 3 — multi-camera + drone observation

- [ ] CAM-001 — Pi 3B camera streaming node → [story](docs/tickets/CAM-001-pi3b-camera-streaming.md)
- [ ] TRK-027 — Multi-camera observation fusion → [story](docs/tickets/TRK-027-multi-camera-fusion.md)
- [ ] TRK-028 — Drone detector module → [story](docs/tickets/TRK-028-drone-detector.md)
- [ ] MAV-001 — MAVLink adapter (ZMQ → VISION_POSITION_ESTIMATE) → [story](docs/tickets/MAV-001-mavlink-adapter.md)

### Phase 4 — laser control

- [ ] LASER-001 — LaserController adapter (Python + serial) → [story](docs/tickets/LASER-001-laser-controller-adapter.md)
- [ ] LASER-002 — MCU firmware (modulation driver) → [story](docs/tickets/LASER-002-mcu-firmware.md)
- [ ] LASER-003 — Closed-loop laser aiming → [story](docs/tickets/LASER-003-closed-loop-aiming.md)
- [ ] LASER-004 — Servo/galvo accuracy characterisation → [story](docs/tickets/LASER-004-servo-characterisation.md)

### Phase 5 — active calibration

- [ ] TRK-029 — ADR-009 observability validation → [story](docs/tickets/TRK-029-observability-validation.md)
- [ ] TRK-030 — Active calibration refinement → [story](docs/tickets/TRK-030-active-calibration-refinement.md)

### Operational

- [ ] META-008 — Install procedure documentation → [story](docs/tickets/META-008-install-procedure.md)
- [ ] META-009 — Operator startup script → [story](docs/tickets/META-009-operator-startup-script.md)
