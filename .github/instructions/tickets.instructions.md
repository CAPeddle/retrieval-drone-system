---
applyTo: "docs/tickets/**/*.md"
description: "Schema and editing conventions for ticket story files in docs/tickets/. Apply when creating or modifying any ticket file."
---
# Ticket Story File Schema

Every story file is a self-contained record of one work item: YAML front-matter for state, required sections for content, an append-only log for history. The story file is the source of truth for the ticket. BOARD.md is only an index — see [board.instructions.md](board.instructions.md).

**When a story file is required:** for tickets at status `next`, `in-progress`, or `blocked`. Backlog tickets MAY be one-line BOARD entries only — create the story file when promoting to Next. Done tickets may keep an existing story file or live as a BOARD line only.

## Front-matter (required at top of file)

```yaml
---
id: PREFIX-NNN                    # Matches filename and BOARD line
status: backlog                   # backlog | next | in-progress | blocked | done
subsystem: tracking-core          # Subsystem name from README.md table, or "docs" / "meta"
tier: mechanical                  # mechanical | small | design
created: 2026-05-31              # ISO date (YYYY-MM-DD)
updated: 2026-05-31              # ISO date — bump on EVERY status change
depends_on: []                    # Ticket IDs that must complete before this starts
spec: null                        # tier=design only. Path to spec doc once written.
plan: null                        # tier=design only. Path to plan doc once written.
blockers: []                      # Free-text reasons when status=blocked
---
```

All fields above are required except `depends_on`, `spec`, `plan`, `blockers` (which default to `[]` / `null`).

### `depends_on` convention

- Lists **ticket IDs only** (e.g., `["TRK-002", "TRK-005"]`).
- Means "this ticket cannot start until all listed tickets reach `done`."
- Unlike `blockers:` (which is free-text describing why a ticket is stuck right now), `depends_on` is a permanent structural fact about execution order.
- Agents determining "what can I work next?" filter for tickets whose `depends_on` list is fully resolved (all IDs are `status: done`).
- A ticket with empty `depends_on: []` has no prerequisite tickets (it may still have external gates documented in the Plan section).

## Required Sections

```markdown
## Context
Why this work matters. One paragraph. For tier=design, restates the spec's headline.

## Acceptance
Falsifiable criteria. A future reader must be able to determine whether each criterion holds. Bullet list.

## Plan
- tier=mechanical: 3–6 numbered shell-level steps, inline.
- tier=small: 5–10 numbered steps, inline.
- tier=design: brief summary; full plan lives in the file linked by `plan:` front-matter.

## Log
Append-only, dated entries. Never delete. Every status transition adds a line.
```

## Status Transition Protocol

**Prefer the script:** `python tools/board/ticket_move.py <ID> <status> [--note "..."]` performs all four steps below atomically. Hand-edit only when the script can't express what you need.

When changing status:

1. Update `status:` and `updated:` in the front-matter.
2. Append a `## Log` entry in the format: `- YYYY-MM-DD: <old> → <new>. <one-sentence reason>.`
3. Move the corresponding line in BOARD.md to the matching column (per [board.instructions.md](board.instructions.md)).
4. Commit the story file edit and BOARD.md edit together as one commit.

Example Log entry:

```markdown
- 2026-06-02: next → in-progress. Started execution after DOCS-001 merged.
```

## Tier Selection

Pick the tier honestly. Forcing tier-1 work through full brainstorming is wasteful; treating tier-3 work as mechanical produces silent architectural drift.

| Question | If yes → tier |
|---|---|
| Is this a `git mv` + reference update with no judgment calls? | `mechanical` |
| Can the full plan be written in 5–10 steps without research? | `small` |
| Are there multiple alternatives to weigh, or unclear acceptance? | `design` |
| Does this touch an ADR, a contract, or a load-bearing decision? | `design` |

When between tiers, choose the heavier one. Easier to skip steps from a `design` ticket than to retrofit a spec after the fact.

## Plan Section Conventions

### `mechanical` and `small` tiers: U-ID sub-tasks

The Plan section uses **U-IDs** (U1, U2, U3, ...) to label each step. These are the units of work an agent executes sequentially.

```markdown
## Plan

U1. Create `src/core/foo.hpp` with class skeleton.
U2. Implement constructor: initialise dependencies.
U3. Implement `process()` method with hot-path discipline.
U4. Add config field: `foo.bar_threshold` (default 0.5).
U5. Unit tests: boundary cases, error paths.
```

Rules:
- Each U-ID is one logical step (one file, one function, one test group).
- Steps are sequentially ordered — U2 depends on U1, etc.
- An agent dispatched to "execute TRK-006" works through U1→U2→...→UN in order.
- U-IDs are stable identifiers for progress tracking ("completed through U4, resuming at U5").

### `design` tier: child tickets

Design-tier tickets do NOT have U-IDs. Instead they have a **sub-ticket table**:

```markdown
## Plan

Tier `design` — spawns sub-tickets for [brief rationale].

### Sub-tickets

| Child | Title | Scope |
|-------|-------|-------|
| TRK-009a | PSD strategy benchmark | ... |
| TRK-009b | Rolling buffer + correlation | ... |

### Orchestration notes for overseer agent

- TRK-009a must complete first.
- TRK-009b and TRK-009c can run in parallel after 009a.
```

Rules:
- Child ticket IDs use letter suffixes: `TRK-NNNa`, `TRK-NNNb`, etc.
- Each child has its own story file with `depends_on:` linking to siblings/parent.
- The parent ticket is "done" when all children are done.
- Orchestration notes tell agents about parallelism and ordering within the group.

## File Naming

`docs/tickets/PREFIX-NNN-short-title-with-hyphens.md`. The slug matches (or is a kebab-case rendering of) the BOARD ticket title. Numbers are not reused. Files are not deleted — Done tickets stay in `docs/tickets/`; only their BOARD line is archived.
