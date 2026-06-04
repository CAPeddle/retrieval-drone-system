---
id: TRK-025
status: backlog
subsystem: tracking-core
tier: design
created: 2026-05-31
updated: 2026-05-31
depends_on:
  - "TRK-021"
spec: null
plan: null
blockers: []
---

## Context

The replay test harness is a v0.3 ship gate (CLAUDE.md §9.2, D-05). It plays recorded sensor data through the production pipeline and compares output against ground truth. The critical acceptance gate: **zero false positives on `safe_for_control`** across at least three recorded scenarios totalling at least 5 minutes of footage. The framework choice is explicitly in the "NOT locked" list (CLAUDE.md §3) — this ticket makes that decision.

## Acceptance

- A replay test framework that:
  - Reads recorded frame sequences from disk (format TBD: raw frames as numbered PNGs/binary, or a video container with frame-accurate seeking).
  - Feeds frames through the full production pipeline (same code paths as live operation).
  - Captures the pipeline's output (TrackingSnapshot per frame) and compares against ground truth.
  - Reports: per-frame `safe_for_control` correctness, false positives (CRITICAL FAIL), false negatives (warning), position accuracy.
- Ground truth format: per-frame JSON with `{ laser_position, ball_position, expected_safe_for_control }`.
- At least 3 recorded scenarios with ground truth (can be synthetic initially, real hardware recordings before ship).
- Zero false positives gate: test FAILS if any frame produces `safe_for_control = true` when ground truth says false.
- False negatives reported with count and percentage (quality metric, not a ship gate).
- Scenario recorder: a tool that captures frames + manual annotations for ground truth generation.
- Runs as part of the test suite but in a separate `replay_tests` CMake target (not blocking per-commit CI due to runtime length).

## Plan

Tier `design` — spawns sub-tickets for framework choice, scenario infrastructure, and validation.

### Sub-tickets

| Child | Title | Scope |
|-------|-------|-------|
| TRK-025a | Replay framework design + selection | Choose format, harness architecture, ground truth schema |
| TRK-025b | Scenario recorder tool | Python tool to capture frames + annotations |
| TRK-025c | Replay harness implementation | C++ harness that feeds recorded frames through pipeline |
| TRK-025d | Initial synthetic scenarios | 3 synthetic scenarios with computed ground truth for CI |

### Orchestration notes for overseer agent

- TRK-025a must complete first (framework decision gates implementation).
- TRK-025b and TRK-025c can proceed in parallel after TRK-025a.
- TRK-025d depends on TRK-025c (harness must exist to validate scenarios).
- Real hardware scenarios are captured later during integration testing on Pi 5 — not part of this ticket tree.

## Log

- 2026-05-31: created. Status: backlog. Tier `design` — resolves D-05. Depends on the full pipeline being assembled (TRK-020c+).
