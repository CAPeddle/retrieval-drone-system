---
description: Ticket story-file schema and BOARD.md editing rules. Applies when creating or modifying tickets or the kanban board.
paths:
  - "docs/tickets/**/*.md"
  - "BOARD.md"
---

# Tickets & Board

Story files are the source of truth for a ticket; BOARD.md is a thin one-line
index. Prefer the scripts in `tools/board/` — they update the story file's
`status:`, `updated:`, `## Log`, and the BOARD line atomically.

- `python tools/board/ticket_new.py PREFIX-NNN "Title" [--tier mechanical|small|design]`
- `python tools/board/ticket_move.py PREFIX-NNN <status> [--note "..."]`
- `python tools/board/board_check.py` — lint BOARD.md against story files; exits 1 on mismatch.
- `python tools/board/ticket_archive.py [--keep N]`

Hand-edit only when the script can't express the change. Full usage in
`tools/board/README.md`.

## Story-file front-matter (required at top)

```yaml
---
id: PREFIX-NNN                    # Matches filename and BOARD line
status: backlog                   # backlog | next | in-progress | blocked | done
subsystem: tracking-core          # Subsystem name from README, or "docs" / "meta"
tier: mechanical                  # mechanical | small | design
created: 2026-05-31               # ISO date (YYYY-MM-DD)
updated: 2026-05-31               # ISO date — bump on EVERY status change
depends_on: []                    # Ticket IDs that must complete before this starts
spec: null                        # tier=design only. Path to spec doc once written.
plan: null                        # tier=design only. Path to plan doc once written.
blockers: []                      # Free-text reasons when status=blocked
---
```

All fields required except `depends_on`, `spec`, `plan`, `blockers` (default
`[]` / `null`). A story file is required for `next` / `in-progress` / `blocked`;
Backlog tickets MAY be a one-line BOARD entry only (create the story file when
promoting to Next); Done tickets may keep an existing file or live as a BOARD
line only.

**`depends_on`** lists ticket IDs only (e.g. `["TRK-002"]`) — "cannot start
until all listed tickets reach `done`". It is a permanent structural fact about
execution order, unlike free-text `blockers:` (why it is stuck right now). To
find next work, filter for tickets whose `depends_on` are all `done`.

## Required sections

```markdown
## Context
Why this work matters. One paragraph. For tier=design, restates the spec's headline.

## Acceptance
Falsifiable criteria a future reader can check. Bullet list.

## Plan
- mechanical: 3–6 numbered shell-level steps, inline.
- small: 5–10 numbered steps, inline.
- design: brief summary; full plan lives in the file linked by `plan:` front-matter.

## Log
Append-only, dated entries. Never delete. Every status transition adds a line.
```

## Status transition protocol

Prefer `ticket_move.py`. When hand-editing, do all four atomically:
1. Update `status:` and `updated:` in front-matter.
2. Append a `## Log` entry: `- YYYY-MM-DD: <old> → <new>. <one-sentence reason>.`
3. Move the BOARD.md line to the matching column.
4. Commit the story-file edit and BOARD.md edit together.

## Tier selection

Pick honestly; when between tiers, choose the heavier one.

| Question | If yes → tier |
|---|---|
| A `git mv` + reference update with no judgment calls? | `mechanical` |
| Full plan in 5–10 steps without research? | `small` |
| Multiple alternatives to weigh, or unclear acceptance? | `design` |
| Touches an ADR, a contract, or a load-bearing decision? | `design` |

**Plan section — `mechanical`/`small`:** use **U-IDs** (U1, U2, …), one logical
step each (one file / function / test group), sequentially ordered and stable
for progress tracking ("completed through U4, resuming at U5"). Do one
exploratory read pass before U1, then execute; if execution reveals complexity
needing new searching, stop and describe the gap first.

**Plan section — `design`:** no U-IDs. Instead a **sub-ticket table** with child
IDs (`TRK-NNNa`, `TRK-NNNb`) each having their own story file and `depends_on`,
plus orchestration notes on parallelism/ordering. The parent is done when all
children are done.

## File naming

`docs/tickets/PREFIX-NNN-short-title-with-hyphens.md`. Numbers are never reused;
files are never deleted (Done tickets stay; only their BOARD line is archived).

## BOARD.md editing

The board is one line per ticket — no inline detail. `board_check.py` verifies
consistency.

**Allowed edits:** add a ticket (one Backlog line + create the story file); move
a ticket (cut/paste line + update story `status:` + append Log); update a
`→ [story](...)` link; archive Done (cut oldest Done lines to
`docs/board-archive.md` with their `(YYYY-MM-DD)` suffix).

**Forbidden:** inline detail; column reordering (fixed order: Next, In Progress,
Blocked, Done (recent), Backlog); bulk edits (one transition per commit); status
drift (BOARD column must match story `status:`); deletion (cancelled tickets get
`status: done` + a Log entry).

**Line format:**
```
- [ ] PREFIX-NNN — Short title → [story](docs/tickets/PREFIX-NNN-slug.md)
- [ ] PREFIX-NNN — Short title → [story](...) (waiting on X)   # blocked
- [x] PREFIX-NNN — Short title (2026-05-31)                    # done, no link required
```

**Concurrent edits:** different tickets moved concurrently → accept both (additive,
line-oriented). Same ticket moved by two agents → later commit wins board
position; both reconcile the story `## Log` so history is complete. If a change
needs more than a single-line board edit, stop — it belongs in the story file or
should be split into commits.
