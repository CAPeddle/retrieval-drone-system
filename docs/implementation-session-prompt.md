# Implementation Session Prompt

> Paste this into a fresh Copilot chat to kick off a v0.3 implementation session.
> Works with zero prior context — all state is in the repo files.
> Adjust the `[SLOT]` markers for your current situation.

---

## The Prompt

```
I want to execute implementation work on the Drone Ball Retrieval tracking system. This is a long-running session — maximise useful output before context fills.

## Bootstrap (do this first, silently)

1. Read `docs/v0.3-execution-guide.md` — this is your map.
2. Read `BOARD.md` — check current column state.
3. Read `/memories/repo/conventions.md` (repo memory) — project conventions.
4. Scan `docs/tickets/` for any ticket with `status: in-progress` — that's unfinished work to resume.
5. If nothing is in-progress: find the next ready ticket(s) using `depends_on:` resolution (all deps done → ready).

## Session mode: ce-work execution

Execute tickets following the U-ID plan steps in each story file. For each ticket:

### Pre-flight (use Explore subagent for research, don't pollute main context)
- Dispatch an Explore subagent to: "Read the story file for [TICKET-ID], all ADRs it references, and the relevant .instructions.md files. Return: acceptance criteria, plan steps, coding standards that apply, and any gotchas from CLAUDE.md §10 (Common Pitfalls) that are relevant."
- If the ticket is `design` tier with sub-tickets: read the parent's orchestration notes first.

### Execution
- Work through U-ID steps sequentially (U1 → U2 → ... → UN).
- Write code that passes the acceptance criteria in the story file.
- After every 2–3 U-IDs: run the build (`cmake --build tracking-core/build`) and tests.
- If a step reveals an unresolved design question: stop, surface it via the question tool, wait for my answer.

### Checkpoint (use question tool after each completed ticket)
Ask me:
- "Ticket [ID] complete — all acceptance criteria met. Review the changes, or continue to next ticket?"
- If I say continue: move the ticket to done (`python tools/board/ticket_move.py [ID] done --note "Implemented"`), then proceed.
- If I say review: pause for my feedback.

### Between tickets
- Run `python tools/board/board_check.py` to verify integrity.
- Identify the next ready ticket from the dependency chain.
- Brief me: "[NEXT-ID]: [title]. Dependencies satisfied. Starting pre-flight." Then loop.

## Quality gates (non-negotiable)
- All C++ code follows `.github/instructions/cpp-hot-path.instructions.md` (no alloc on hot path, no exceptions, pre-allocated buffers).
- All Python follows `.github/instructions/python-tooling.instructions.md` (black, ruff, pydantic for schemas).
- Every new module has unit tests that would fail if the acceptance criteria were violated.
- ADRs are cited by ID in code comments where they motivate a design choice.
- British English in all prose.

## Sub-agent dispatch strategy
- **Explore** (read-only research): use liberally for reading ADRs, checking existing code patterns, looking up OpenCV/ZMQ API details. Keeps main context clean.
- **ce-best-practices-researcher**: before implementing a novel component (e.g., SPSC queue, PSD correlation), dispatch to research proven patterns and return a recommendation.
- **ce-correctness-reviewer** + **ce-performance-reviewer**: after completing each ticket's code, dispatch both as a mini code-review before moving on.
- **ce-testing-reviewer**: after writing tests, dispatch to check for coverage gaps.

## What NOT to do
- Don't read CLAUDE.md in full (it's 500+ lines; the execution guide + repo memory have what you need).
- Don't re-plan — the plan exists in each story file. Execute it.
- Don't refactor adjacent code. Stay scoped to the ticket.
- Don't commit without my explicit approval.
- Don't skip tests to "move faster".

## Current state: [FILL THIS IN]

[Pick ONE of these and delete the others:]

**Option A — Fresh start (no tickets done yet):**
Start with TRK-002 (Build system overhaul). This is the foundation — nothing else can proceed until it's done.

**Option B — Resuming mid-stream:**
The following tickets are done: [LIST DONE IDs]. Resume from the next ready ticket in the dependency chain.

**Option C — Specific ticket:**
Execute ticket [ID] only. I'll tell you when to stop.

Begin.
```

---

## Variants

### Parallel-ticket variant (add if multiple independent tickets are ready)

```
Multiple tickets are ready in parallel. Execute them in this order but use subagents to pre-research the next ticket while I review the current one:
1. [ID-1] — [title]
2. [ID-2] — [title]
3. [ID-3] — [title]
```

### Research-heavy variant (for design-tier tickets)

```
This is a design-tier ticket. Before executing:
1. Dispatch ce-best-practices-researcher to research: "[specific technology question]"
2. Dispatch ce-web-researcher if external API docs or library comparisons are needed.
3. Present me with findings + your recommendation via the question tool.
4. Only proceed to implementation after I confirm the approach.
```

### Review-focused variant (for checking existing work)

```
Don't implement — review. For each ticket marked `done` in BOARD.md:
1. Read its story file acceptance criteria.
2. Dispatch ce-correctness-reviewer + ce-performance-reviewer on the implementation.
3. Report: which criteria are met, which are not, and what's missing.
```
