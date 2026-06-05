---
description: Find the next ready ticket (all dependencies done)
agent: build
---

Execution map:
@docs/v0.3-execution-guide.md

Board:
@BOARD.md

Story files live in `docs/tickets/`. Find the next **ready** ticket: the leftmost un-done ticket in the recommended execution order whose every `depends_on:` ticket is `status: done`. Read story-file frontmatter as needed to resolve dependency statuses.

Report: ticket ID, title, `tier:`, and the status of each dependency. If the ticket is `design` tier, note that its design phase (brainstorming → spec → plan) routes to Claude Code, not OpenCode. Do not start the ticket.
