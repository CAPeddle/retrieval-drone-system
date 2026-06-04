---
id: META-007
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

After building the board automation scripts ([META-006](META-006-build-board-automation-scripts.md)) they were technically present but invisible to fresh agent sessions. Claude Code only auto-loads [CLAUDE.md](../../CLAUDE.md); GitHub Copilot only auto-loads [.github/copilot-instructions.md](../../.github/copilot-instructions.md) and file-scoped instruction files matching the file being edited. Neither auto-reads [`tools/board/README.md`](../../tools/board/README.md). A fresh agent asked to "move DOCS-001 to In Progress" would hand-edit BOARD.md and the story file (probably correctly, but slowly and non-atomically) because nothing in their auto-loaded context pointed at the scripts.

This ticket plants pointers in every auto-loaded location and creates an [AGENTS.md](../../AGENTS.md) cross-vendor entry point so the scripts are reachable from every agent's first read.

## Acceptance

- [CLAUDE.md](../../CLAUDE.md) has a new §11.7 "Repo tooling" pointing at [tools/board/README.md](../../tools/board/README.md) with a script summary.
- [README.md](../../README.md) Workflow section includes a "Tooling" callout before the Risk-register paragraph.
- [.github/copilot-instructions.md](../../.github/copilot-instructions.md) Work Tracking section includes a Helpers bullet pointing at the scripts.
- [.github/instructions/board.instructions.md](../../.github/instructions/board.instructions.md) has a "Prefer the scripts" section above "Allowed Edits".
- [.github/instructions/tickets.instructions.md](../../.github/instructions/tickets.instructions.md) Status Transition Protocol has a "Prefer the script" line above the manual 4-step protocol.
- [AGENTS.md](../../AGENTS.md) exists at the repo root as a cross-vendor entry point: cardinal rules, agent-to-auto-loaded-doc mapping, workflow summary, common operations table, contradiction-resolution rule.
- `python tools/board/board_check.py` continues to exit 0.

## Plan

1. Add §11.7 to CLAUDE.md.
2. Insert Tooling callout in README.md Workflow section.
3. Append Helpers bullet to .github/copilot-instructions.md Work Tracking.
4. Insert "Prefer the scripts" callouts in board.instructions.md and tickets.instructions.md.
5. Create AGENTS.md at repo root.
6. Run `python tools/board/board_check.py` to verify.

## Log

- 2026-05-31: created. Status: in-progress.
- 2026-05-31: in-progress → done. Pointers planted in all five auto-loaded locations, AGENTS.md created, board_check passes.
