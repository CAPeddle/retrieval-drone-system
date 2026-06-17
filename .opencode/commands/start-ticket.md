---
description: Begin a ticket — tier gate, pre-flight research, move to in-progress
agent: build
---

Ticket: $1

Do these in order:

1. **Locate** the story file for `$1` under `docs/tickets/` and read its `tier:` frontmatter.
2. **Tier gate.** If `tier: design`, STOP. Tell me to run the design phase (brainstorming → spec → plan) in **Claude Code** first, because design work routes to the Claude subscription. Do not move the ticket, do not write code.
3. **Pre-flight.** Dispatch the `@explore` subagent on `$1` and require an evidence-first brief: acceptance criteria, Plan U-ID steps, coding standards that apply, ADRs in play, evidence paths, and any relevant CLAUDE.md §10 pitfalls.
4. **Context packet.** Before writing code, compress the pre-flight into a bounded execution packet with exactly these sections:
	- `Goal` — 1 sentence.
	- `Acceptance targets` — 3 to 7 bullets.
	- `ADRs in play` — at most 3 ADR IDs, each with the clause if relevant and the source path used to verify it.
	- `Files in scope` — at most 6 paths.
	- `Open questions` — at most 3 bullets.
	If you cannot fit the work inside this packet without dropping material facts, STOP and ask me to trim or refocus the scope before coding.
5. **Tool plan.** Before implementation, state the minimal tool sequence you will use. For each planned read/search/edit/command, include: why it is needed, the expected output, and the stop condition. Do one exploratory read/search pass only, then move to execution. Do not keep searching once the controlling code path and discriminating check are known.
6. **Move.** Run: `python tools/board/ticket_move.py $1 in-progress --note "Starting execution"`
7. **Brief me** with the context packet and tool plan, then wait for my go-ahead before writing any code.

Confirm all `depends_on:` tickets are `status: done` before step 6. If any dependency is open, stop and tell me.
