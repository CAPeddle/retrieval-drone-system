---
id: META-012
status: backlog
subsystem: meta
tier: mechanical
created: 2026-06-05
updated: 2026-06-05
depends_on: []
spec: null
plan: null
blockers: []
---
## Context

The agent-native audit flagged an action-parity inconsistency: `ticket_archive.py` is allow-listed in `opencode.json` and documented in `tools/board/README.md`, but unlike the other board operations it has no `.opencode/commands/` wrapper. Every other routine board operation is reachable as a slash command. A thin `/ticket-archive` command restores parity.

## Acceptance

- `.opencode/commands/ticket-archive.md` exists, runs `python tools/board/ticket_archive.py --keep 10`, and summarises which tickets moved to `docs/board-archive.md`.
- The command file follows the same frontmatter and structure as the existing command files (e.g. `.opencode/commands/board.md`).
- The `--keep` value is stated and easily editable; the command notes that archival is a move (not a delete).

## Plan

U1. Read `.opencode/commands/board.md` for the house structure and `ticket_archive.py` for its exact flags/output.
U2. Write `.opencode/commands/ticket-archive.md`: frontmatter `description:`, the run step, and the summarise step.
U3. Confirm the command name and behaviour are consistent with the README Scripts table.

## Log

- 2026-06-05: created. Status: backlog. Source: agent-native audit action-parity finding (missing command wrapper).
