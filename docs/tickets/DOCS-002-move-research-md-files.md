---
id: DOCS-002
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

Five large brainstorm `.md` files sit at the repo root from v0.2-era thinking. They are rank-5 reference material per [CLAUDE.md §6](../../CLAUDE.md), not specification. Mixing them at root makes it harder to tell governance docs (CLAUDE.md, README.md, BOARD.md) from historical research. Move to `docs/research/` where they're clearly marked as reference.

The files to move:

- `ChatGPT Tracking Visualisation System Design.md`
- `CopilotServoCOntrol.md`
- `DIY Drone Electronics for Ball Retrieval.md`
- `Gemini Building a Home Drone - Design Brainstorm.md`
- `Gemini Drone Ball Retrieval System Design Review.md`

## Acceptance

- `docs/research/` exists and contains all 5 files listed above.
- The repo root contains only governance `.md` files: `CLAUDE.md`, `README.md`, `BOARD.md`.
- `git log --follow` on any moved file shows pre-move history.
- References to the moved filenames in `CLAUDE.md` (search for `Gemini`, `ChatGPT`, `DIY Drone`, `CopilotServoCOntrol`) and `README.md` are updated to the new `docs/research/` paths.
- `rg "Gemini Drone Ball|Gemini Building a Home|DIY Drone Electronics|CopilotServoCOntrol|ChatGPT Tracking"` returns no stale path references.

## Plan

1. `mkdir -p docs/research`
2. `git mv "ChatGPT Tracking Visualisation System Design.md" docs/research/`
3. `git mv "CopilotServoCOntrol.md" docs/research/`
4. `git mv "DIY Drone Electronics for Ball Retrieval.md" docs/research/`
5. `git mv "Gemini Building a Home Drone - Design Brainstorm.md" docs/research/`
6. `git mv "Gemini Drone Ball Retrieval System Design Review.md" docs/research/`
7. Update references in `CLAUDE.md` (multiple sections cite these by name) and `README.md` (Pointers section).
8. Verify with `rg` per Acceptance.
9. Commit as one change: `docs: move v0.2 research .md files to docs/research/`.

## Log

- 2026-05-31: created. Status: next.
