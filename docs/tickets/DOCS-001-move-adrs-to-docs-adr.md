---
id: DOCS-001
status: next
subsystem: docs
tier: mechanical
created: 2026-05-31
updated: 2026-05-31
spec: null
plan: null
blockers: []
---

## Context

[CLAUDE.md §8.7](../../CLAUDE.md) prescribes `docs/adr/` as the home for Architecture Decision Records. The ADRs currently live in `Claude Synthesised/` — the literal name from when they were dumped into the repo. The mismatch causes path drift across CLAUDE.md, [.github/copilot-instructions.md](../../.github/copilot-instructions.md), [.github/instructions/adr-governance.instructions.md](../../.github/instructions/adr-governance.instructions.md), README.md, and BOARD.md. Sequenced first because every other DOCS ticket touches ADR references.

## Acceptance

- `docs/adr/` exists with all 10 ADR files plus `README.md` (the ADR index).
- `Claude Synthesised/` no longer exists in the working tree.
- `git log --follow docs/adr/ADR-001-hybrid-cpp-python.md` shows pre-move history (i.e. `git mv` was used, not delete+add).
- `rg "Claude Synthesised"` returns no matches in tracked files except in this story file's `## Log` and `docs/board-archive.md` (if it exists).
- The ADR README's "See also" link to the consolidated design doc is preserved.

## Plan

1. `mkdir -p docs/adr`
2. `git mv "Claude Synthesised"/*.md docs/adr/`
3. `rmdir "Claude Synthesised"` (must be empty after step 2)
4. Search and update all references with `rg "Claude Synthesised"` — expected hits in: `CLAUDE.md`, `.github/copilot-instructions.md`, `.github/instructions/adr-governance.instructions.md`, `README.md`, `BOARD.md`, this story file's Context.
5. Verify `rg "Claude Synthesised"` returns zero matches in tracked files.
6. Commit as one change: `docs: move ADRs from "Claude Synthesised/" to docs/adr/`.

## Log

- 2026-05-31: created. Status: next. Sequenced first; every other DOCS ticket references this path.
