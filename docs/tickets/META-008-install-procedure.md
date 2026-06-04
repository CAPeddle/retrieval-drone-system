---
id: META-008
status: backlog
subsystem: meta
tier: small
created: 2026-05-31
updated: 2026-05-31
depends_on: []
spec: null
plan: null
blockers: []
---

## Context

CLAUDE.md §4 (Physical Environment) and §13.2 R-05 (camera focus drift) reference an install procedure that does not yet exist. The system has physical preconditions that must be met before first run: camera focus locked, Charuco board placed, floor flatness validated, exposure settings configured. Without a documented procedure, operators (future self) will skip steps and produce subtle calibration errors.

## Acceptance

- A document at `docs/runbooks/install-procedure.md` covering:
  1. Hardware assembly checklist (camera mounted, focus ring locked, CSI cable seated).
  2. Charuco board placement (primary corner at floor frame origin, board level).
  3. Floor flatness validation procedure (board at N positions, run flatness check script, assert < threshold).
  4. Camera exposure configuration (`v4l2-ctl` commands, verify with test capture).
  5. Initial calibration run (invoke TRK-012 intrinsics tool, then TRK-013 extrinsics tool).
  6. Verification: run tracking core, observe known object positions, confirm < 5mm error.
- Each step has a "verify" sub-step (how to know it succeeded).
- Warnings for common mistakes (focus ring not locked, Charuco not level, wrong exposure mode).
- References to relevant ADRs (ADR-004 Phase 1, ADR-006 origin).

## Plan

U1. Create `docs/runbooks/install-procedure.md` with section skeleton.
U2. Write hardware assembly checklist.
U3. Write Charuco placement + floor origin procedure.
U4. Write floor flatness validation procedure (references future flatness script from R-02 mitigation).
U5. Write camera exposure configuration section with exact `v4l2-ctl` commands.
U6. Write initial calibration section referencing TRK-012 and TRK-013 tools.
U7. Write verification section with expected outcomes and troubleshooting.

## Log

- 2026-05-31: created. Status: backlog. No hard dependencies; can be written once TRK-012/013 tools exist.
