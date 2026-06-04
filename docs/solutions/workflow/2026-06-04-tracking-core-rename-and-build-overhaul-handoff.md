---
title: Tracking Core Rename + Build Overhaul Handoff Pattern
captured: 2026-06-04
applies_to: [tracking-core]
tags: [workflow, build-system, handoff, ticket-state, validation]
problem_type: recipe
source: internal
---

## Summary

Use this pattern when a structural change (directory rename + build-system overhaul) is implemented in one environment but final verification must happen on target hardware. The key is to complete implementation artifacts, mark precise blockers, and leave a deterministic handoff for the next execution environment.

## Context

TRK-001 renamed the subsystem from tracking-system to tracking-core. TRK-002 then overhauled CMake targets and dependency wiring (FetchContent + system OpenCV). Implementation work completed, but the local Windows environment lacked OpenCV/libzmq/cv2, so final build and test validation had to be deferred to Pi 5 or Linux.

## Approach

1. Execute structural change first (`git mv`) and update all path references in docs, instruction `applyTo` globs, editor settings, and build scripts.
2. Implement build-system target split (`tracking_core_lib`, `tracking_core`, `tracking_core_tests`) before any feature work.
3. Run verification commands anyway in the local environment to distinguish path/logic errors from environment-missing dependency errors.
4. Move the active ticket to `blocked` when implementation is complete but acceptance cannot be fully verified in current environment.
5. Store blocker detail in both board line and ticket frontmatter `blockers:`.
6. Capture a solutions entry immediately so a fresh session can resume without chat history.

## Trade-offs

- Good: prevents false confidence and keeps board truth aligned with actual verification state.
- Good: future sessions can resume from repo docs only.
- Cost: one extra unblock step is required before dependent tickets can start.
- Risk: if blocker notes are vague, later sessions may repeat failed local validation attempts.

## Worked Example

- Completed work:
  - `tracking-core/CMakeLists.txt` rewritten with C++17, warning flags, FetchContent dependencies.
  - `tracking-core/src/core/CMakeLists.txt` split into library + executable targets.
  - `tracking-core/tests/cpp_unit/CMakeLists.txt` linked against `tracking_core_lib`.
  - `tracking-core/README.md` updated with Pi 5/Linux prerequisites.
- Block condition:
  - Local Windows environment missing OpenCV/libzmq/cv2.
- Documentation actions:
  - `TRK-002` moved to `blocked` with explicit blocker reason.
  - Board line updated with the same blocker summary.
  - This solution recipe added under `docs/solutions/workflow/`.

## Source Links

- `docs/tickets/TRK-001-rename-tracking-system-to-tracking-core.md`
- `docs/tickets/TRK-002-build-system-overhaul.md`
- `tracking-core/CMakeLists.txt`
- `tracking-core/src/core/CMakeLists.txt`
- `tracking-core/tests/cpp_unit/CMakeLists.txt`
- `BOARD.md`
