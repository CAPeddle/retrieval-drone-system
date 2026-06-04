---
id: META-006
status: done
subsystem: meta
tier: small
created: 2026-05-31
updated: 2026-05-31
spec: null
plan: null
blockers: []
---

## Context

The workflow documented in [README → Workflow](../../README.md#workflow) requires multiple coordinated edits per ticket transition: update story file `status:`, bump `updated:`, append a `## Log` entry, move the BOARD line. Doing these by hand is tedious and easy to get wrong (e.g., forgetting the Log line or moving the BOARD line into the wrong column). This ticket captures the work of building stdlib-only Python helpers — invokable from any agent's shell — that automate the mechanics.

## Acceptance

- `tools/board/` exists with: `_lib.py`, `ticket_new.py`, `ticket_move.py`, `board_check.py`, `ticket_archive.py`, `README.md`.
- All scripts are stdlib-only (no `pip install` step).
- `python tools/board/board_check.py` exits 0 against the current board.
- `tools/board/README.md` documents usage, valid prefixes/statuses, and known limitations.
- The two file-scoped instruction docs (`board.instructions.md`, `tickets.instructions.md`) softened to reflect that Backlog lines may exist without a story file.

## Plan

1. Build `_lib.py` (shared parsing: frontmatter, board lines, prefix maps, slugify, today).
2. Build `ticket_new.py` (scaffold story file + append Backlog line).
3. Build `ticket_move.py` (update story frontmatter + Log + move BOARD line).
4. Build `board_check.py` (lint BOARD ↔ story status agreement).
5. Build `ticket_archive.py` (trim Done > N to `docs/board-archive.md`).
6. Write `tools/board/README.md`.
7. Soften `board.instructions.md` and `tickets.instructions.md` to allow Backlog one-liners without story files.
8. Run `python tools/board/board_check.py` and verify exit 0.

## Log

- 2026-05-31: created. Status: in-progress.
- 2026-05-31: in-progress → done. Scripts built, instructions softened, board_check passes.
