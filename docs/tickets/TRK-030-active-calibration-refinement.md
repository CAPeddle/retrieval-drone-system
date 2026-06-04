---
id: TRK-030
status: backlog
subsystem: tracking-core
tier: design
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-029"
spec: null
plan: null
blockers: []
---

## Context

If ADR-009 passes the promotion gates (TRK-029 observability + LASER-004 servo accuracy), this ticket implements active calibration refinement: the system periodically points the laser at known floor positions, observes the dot, and refines its homography. This keeps the extrinsic calibration accurate despite thermal drift, camera bumps, or gradual floor-frame shifts.

Key architectural constraints (from ADR-009 Proposed):
- Active calibration runs during **idle periods only** (ball not tracked, drone not flying, `safe_for_control = false`).
- Calibration commands flow: tracking core publishes "request probe at position X" → LaserController aims → tracking core observes result → refines homography.
- The tracking core **never commands the laser directly** (ADR-008 boundary). It publishes a calibration request; the LaserController adapter executes.
- Refinement is bounded: if the update would change the homography by more than a configurable threshold, it is rejected as an outlier (prevents a broken observation from corrupting calibration).
- Status: `calibration_status` transitions to VALID after successful refinement, DEGRADED if too many probes fail.

## Acceptance

- Active calibration loop:
  1. Core publishes `CalibrationProbeRequest` (target floor position) on a dedicated ZMQ topic.
  2. LaserController aims laser at target.
  3. Core observes laser dot, computes floor position via current homography.
  4. Core computes residual (observed floor pos − requested floor pos).
  5. Core refines homography from accumulated residuals (batch or incremental update).
- Probe scheduling: configurable interval (default: every 5 minutes during idle), configurable probe count per cycle (default: 5 positions).
- Outlier rejection: probe residual > `max_probe_residual_m` (default 0.01m) → rejected, logged.
- Convergence criterion: mean residual < `cal_converge_m` (default 0.002m) → VALID.
- Rollback: if refinement increases overall residual, revert to previous homography.
- Integration test: simulated drift → calibration loop converges → accuracy recovers.

## Plan

Tier `design` — architectural integration across core + adapter + MCU.

### Sub-tickets (to be created after ADR-009 promotion)

| Child | Title | Scope |
|-------|-------|-------|
| TRK-030a | CalibrationProbeRequest ZMQ topic | Message schema, publish/subscribe contract |
| TRK-030b | Probe scheduler | Idle detection, interval timing, position selection |
| TRK-030c | Homography refinement engine | Incremental update from probe residuals, outlier rejection |
| TRK-030d | Rollback + convergence logic | Revert on regression, declare VALID on convergence |
| TRK-030e | End-to-end integration test | Simulated drift scenario, verify convergence |

## Log

- 2026-05-31: created. Status: backlog. Phase 5. Gated on: ADR-009 promoted to Accepted (requires TRK-029 + LASER-004 passing).
