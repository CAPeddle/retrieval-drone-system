# DOCS-006: Layout Reconciliation Design

## Status

Accepted for implementation on 2026-06-21.

## Problem

`README.md` defines the chosen multi-subsystem repository layout, but `CLAUDE.md §8.7`, `.github` agent instructions, and scheduled ticket guidance still contained stale single-project path guidance. Future agents could be routed toward root `core/` or `services/` directories even though the project has moved to top-level subsystem directories such as `tracking-core/`, `viewer/`, `laser-controller/`, and `mavlink-adapter/`.

## Requirements

- `CLAUDE.md §8.7` must describe the multi-subsystem layout without treating the old root `core/` and `services/` directories as canonical.
- Active agent instructions must distinguish current transitional paths from target subsystem paths.
- Scheduled adapter/viewer tickets must not direct future implementation into stale `services/` paths.
- The reconciliation must preserve the real root `tools/board/` workflow tooling.
- No physical directories are moved as part of DOCS-006.

## Scope Boundaries

- `VIEW-001` remains responsible for promoting `tracking-core/src/viewer/` to `viewer/`.
- `DOCS-005` remains responsible for broader Copilot instruction modernisation beyond layout contradictions.
- ADR-001, ADR-002, and ADR-008 process boundaries are not reopened.

## Decisions

- Use `README.md` as the baseline for the target multi-subsystem layout.
- Keep board helper scripts under root `tools/board/` because they are repository workflow tooling, not tracking-core implementation tooling.
- Store the implementation plan in `docs/superpowers/plans/` to match the workflow artefact convention documented in `README.md`.
