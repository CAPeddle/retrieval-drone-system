---
description: Begin a ticket — tier gate, pre-flight research, move to in-progress
agent: build
---

Ticket: $1

Do these in order:

1. **Locate** the story file for `$1` under `docs/tickets/` and read its `tier:` frontmatter.
2. **Tier gate.** If `tier: design`, STOP. Tell me to run the design phase (brainstorming → spec → plan) in **Claude Code** first, because design work routes to the Claude subscription. Do not move the ticket, do not write code.
3. **Pre-flight.** Dispatch the `@explore` subagent on `$1` for a context-packet brief. If the context packet cannot fit the work without dropping material facts, STOP and ask me to trim or refocus the scope before coding.
4. **Move.** Run: `python tools/board/ticket_move.py $1 in-progress --note "Starting execution"`
5. **Brief me** with the context packet, then wait for my go-ahead before writing any code.

Confirm all `depends_on:` tickets are `status: done` before step 4. If any dependency is open, stop and tell me.
