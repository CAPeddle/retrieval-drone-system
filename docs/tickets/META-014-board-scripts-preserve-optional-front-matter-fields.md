---
id: META-014
status: done
subsystem: meta
tier: mechanical
created: 2026-07-16
updated: 2026-07-16
spec: null
plan: null
blockers: []
---
## Context

`tools/board/_lib.py` parses story front-matter into a fixed nine-field
dataclass and renders it back from that same fixed list, so any other key is
silently deleted on the next `ticket_move.py` run. This ate TRK-004's
`depends_on` twice during the 2026-07-16 session — `depends_on` is a schema
field (`.claude/rules/tickets.md`) that dependency tooling and "what's next"
filtering rely on, and multi-line YAML lists were not parsed at all.

## Acceptance

- `ticket_move.py` on a story with a multi-line `depends_on:` list leaves the
  list intact (round-trip verified by a runnable test).
- Unknown/extra front-matter keys survive a move instead of being deleted.
- `python3 tools/board/test_lib_frontmatter.py` passes; `board_check.py` still
  passes on the live board.

## Plan

1. Add `depends_on` as a first-class `Frontmatter` field plus an ordered
   `extras` dict for unknown keys.
2. Teach `parse_frontmatter` to collect multi-line `- item` list values.
3. Render `depends_on` in schema position (after `updated`) and extras after
   `blockers`.
4. Add `tools/board/test_lib_frontmatter.py` (stdlib unittest) covering the
   round-trips; run it and `board_check.py`.

## Log

- 2026-07-16: created. Status: backlog.
- 2026-07-16: backlog → done. Fixed in _lib.py: depends_on first-class + extras dict + multi-line list parsing; 7/7 round-trip tests; verified live on TRK-004.
