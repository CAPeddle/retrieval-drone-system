---
id: META-013
status: done
subsystem: meta
tier: mechanical
created: 2026-06-05
updated: 2026-06-05
spec: null
plan: null
blockers: []
---
## Context

The agent-native audit's tools-as-primitives review noted that the `opencode.json` bash allow-list uses a glob, `"python tools/board/*": "allow"` (and the `python3` variant), which auto-approves any script placed in that directory, including a future or malicious one. A least-privilege improvement is to enumerate the four known board scripts explicitly so only the intended commands run without a prompt.

## Acceptance

- `opencode.json` replaces the `python tools/board/*` and `python3 tools/board/*` glob allow entries with explicit per-script entries for `ticket_new.py`, `ticket_move.py`, `board_check.py`, and `ticket_archive.py` (both `python` and `python3` forms, preserving argument wildcards).
- Any script not in that set falls through to the default `"*": "ask"` rule rather than being silently allowed.
- `opencode.json` remains valid JSON and the four board scripts still run without an approval prompt.

## Plan

U1. List the current board scripts to fix the exact set of allowed entries.
U2. Edit `opencode.json`: remove the two glob entries, add explicit `python tools/board/<script>.py *` and `python3 tools/board/<script>.py *` allow entries for each of the four scripts.
U3. Validate JSON (parse) and sanity-check that a non-listed `python tools/board/foo.py` would hit the `ask` default.

## Log

- 2026-06-05: created. Status: backlog. Source: agent-native audit tools-as-primitives finding (glob allow-list).
- 2026-06-05: backlog → done. OpenCode bash allow-list tightened to explicit board scripts.
