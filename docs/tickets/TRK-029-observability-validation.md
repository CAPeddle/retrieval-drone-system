---
id: TRK-029
status: backlog
subsystem: tracking-core
tier: design
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "LASER-004"
spec: null
plan: null
blockers: []
---

## Context

ADR-009 (Proposed) describes using the laser as a calibration probe: point the laser at known floor positions, observe where the tracker reports the dot, and refine the extrinsic calibration online. Before ADR-009 can be promoted to Accepted, two gates must pass:

1. **Observability validation** (this ticket): prove in simulation that the joint optimisation (homography refinement from laser observations) is well-conditioned — i.e., a finite set of laser probe positions produces a unique and accurate homography update.
2. **Servo accuracy characterisation** (LASER-004): prove that the servo can point the laser to known floor positions with sufficient repeatability.

This ticket is the observability analysis. It does NOT implement active calibration — it validates whether it's theoretically sound.

## Acceptance

- A simulation / analysis script (Python) that:
  - Models the camera-floor geometry (known intrinsics, approximate extrinsics).
  - Simulates laser dot observations at N probe positions with realistic measurement noise.
  - Runs homography refinement from the simulated observations.
  - Quantifies: condition number of the normal equations, sensitivity to noise, minimum number of probes needed, optimal probe placement.
- Report documenting:
  - Whether the problem is observable (rank of Jacobian = number of free parameters).
  - Sensitivity: how much does 1-pixel detection noise translate to homography parameter error?
  - Minimum probe count: below what N does the solution become ill-conditioned?
  - Recommended probe pattern: which floor positions give the best conditioning?
- Pass criteria: the optimisation is observable with ≤ 9 probe positions and the expected detection noise (0.5 pixel σ) produces < 2mm floor position error.
- If pass criteria are NOT met: document why and recommend whether to modify ADR-009 or abandon active calibration.

## Plan

Tier `design` — primarily analysis/simulation work.

### Sub-tickets (to be created when Phase 5 begins)

| Child | Title | Scope |
|-------|-------|-------|
| TRK-029a | Simulation model | Camera + floor + laser geometry model |
| TRK-029b | Observability analysis | Jacobian rank, condition number, sensitivity |
| TRK-029c | Probe pattern optimisation | Minimum count, optimal placement |
| TRK-029d | Report + ADR-009 promotion decision | Document results, recommend promote/modify/abandon |

## Log

- 2026-05-31: created. Status: backlog. Phase 5 gate. Gates ADR-009 promotion. Depends on LASER-004 (characterisation results inform noise model).
