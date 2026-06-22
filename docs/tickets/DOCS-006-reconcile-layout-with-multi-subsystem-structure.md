---
id: DOCS-006
status: done
subsystem: docs
tier: design
created: 2026-06-21
updated: 2026-06-21
spec: docs/superpowers/specs/DOCS-006-layout-reconciliation-design.md
plan: docs/superpowers/plans/DOCS-006-layout-reconciliation-plan.md
blockers: []
---

## Context

`README.md` now describes the chosen multi-subsystem repository layout, but `CLAUDE.md §8.7` and related agent instructions still describe an older single-project layout with root `core/`, `services/`, `tools/`, `tests`, and `config` directories. This creates contradictory guidance for future agents and implementation tickets.

## Acceptance

- `CLAUDE.md §8.7` describes the chosen multi-subsystem layout from `README.md` rather than the stale single-project layout.
- Agent-facing instructions that mention implementation locations are consistent with the same layout model, or explicitly mark transitional current paths.
- Scheduled tickets for adapter/viewer work do not direct future implementation into stale `services/` paths unless that directory is intentionally reintroduced by a later decision.
- No physical directory moves are performed as part of this ticket; `VIEW-001` remains the viewer promotion ticket.
- `python3 tools/board/board_check.py` passes after the ticket is moved.

## Plan

See `docs/superpowers/plans/DOCS-006-layout-reconciliation-plan.md`.

## Log

- 2026-06-21: created. Status: backlog.
- 2026-06-21: backlog → in-progress. Started layout reconciliation implementation.
- 2026-06-21: in-progress → done. Reconciled layout guidance across operating docs, agent instructions, and scheduled tickets.
