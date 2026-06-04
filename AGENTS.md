# AGENTS.md

Cross-vendor operating instructions for AI coding agents working in this repository (Claude Code, GitHub Copilot, Cursor, Codex, OpenCode, and any other agent that respects an `AGENTS.md` convention).

This file is the **entry point**. Each agent type also has its own auto-loaded instruction file; see [Where the deeper docs live](#where-the-deeper-docs-live) below.

## Project

The Drone Ball Retrieval System — a multi-subsystem hobbyist project: ground-based camera tracking core (Pi 5), steerable laser, autonomous drone. The currently-active subsystem is [`tracking-core/`](tracking-core/). See [README.md](README.md) for the full subsystem map and [CLAUDE.md](CLAUDE.md) for the full operating contract.

## Cardinal rules

1. **Architecture is governed by ADRs.** Accepted ADRs live in [`Claude Synthesised/`](Claude%20Synthesised/) (→ `docs/adr/` after [DOCS-001](docs/tickets/DOCS-001-move-adrs-to-docs-adr.md) lands). They are not relitigated without explicit user invitation. Reference by ID (e.g. "ADR-007 clause 3"), not paraphrase.
2. **Story files are the source of truth for tickets.** [BOARD.md](BOARD.md) is a thin index. Detail, status, and history all live in `docs/tickets/<ID>-<slug>.md`.
3. **Use the board scripts.** [`tools/board/ticket_new.py`](tools/board/ticket_new.py), [`ticket_move.py`](tools/board/ticket_move.py), [`board_check.py`](tools/board/board_check.py), [`ticket_archive.py`](tools/board/ticket_archive.py) perform atomic status + log + board updates. See [tools/board/README.md](tools/board/README.md). Hand-edits are allowed when the script can't express the change, but scripts are the default.
4. **Never commit unless the user asks.** Even when you've written files. Even after a "looks good".
5. **British English in prose** ("visualisation", "behaviour", "modulisation"). The codebase is consistent.

## Where the deeper docs live

| Agent | Auto-loads on session start | What to do |
|---|---|---|
| **Claude Code** | [CLAUDE.md](CLAUDE.md) | Authoritative; already in your context |
| **GitHub Copilot** | [.github/copilot-instructions.md](.github/copilot-instructions.md), plus file-scoped [`.github/instructions/*.instructions.md`](.github/instructions/) matching the file you're editing | Both authoritative; auto-loaded when relevant |
| **Cursor / Codex / OpenCode / others** | This file (`AGENTS.md`) | Follow this file; load [CLAUDE.md](CLAUDE.md) on demand for deeper context |

If you're a Claude Code or Copilot session, your auto-loaded docs are already in your context — this file is reinforcement, not duplication.

## Workflow at a glance

Work flows through [BOARD.md](BOARD.md). Columns top-to-bottom: **Next → In Progress → Blocked → Done (recent) → Backlog**. Each ticket has an ID with a per-subsystem prefix:

- Subsystems: `TRK`, `VIEW`, `CAM`, `LASER`, `MAV`, `DRONE`
- Cross-cutting: `DOCS`, `META`

**Three tiers** govern process weight (pick the tier honestly — heavier when between):

| Tier | When | Process |
|---|---|---|
| `mechanical` | `git mv`, reference updates, single-commit cleanup | Inline plan in story file. Execute. |
| `small` | 5–10 hand-codable steps, no architectural decisions | Hand-written plan in story file. No formal spec. |
| `design` | Architectural impact, multiple alternatives, unclear acceptance | Brainstorming step → spec file → plan file → implement. |

**Common operations (always prefer these to hand-editing):**

| Task | Command |
|---|---|
| Add a ticket | `python tools/board/ticket_new.py PREFIX-NNN "Title" [--tier mechanical\|small\|design]` |
| Move a ticket | `python tools/board/ticket_move.py PREFIX-NNN <status> [--note "reason"]` |
| Lint the board | `python tools/board/board_check.py` |
| Archive Done | `python tools/board/ticket_archive.py --keep 10` |

Valid statuses: `backlog`, `next`, `in-progress`, `blocked`, `done`.

## When you contradict an existing decision

If a user instruction in the current conversation contradicts an accepted ADR, an existing story file, or a documented rule:

1. **Do not silently override.** Surface the contradiction.
2. Decide whether the rule needs updating (it might).
3. If yes: update the doc *first*, then act on the new direction.

See [CLAUDE.md §6](CLAUDE.md) for the full Source of Truth Hierarchy.

## Solutions library

[`docs/solutions/`](docs/solutions/) holds patterns, recipes, and reviews captured from prior work — including [a critical review of EveryInc's compound-engineering plugin](docs/solutions/external-reviews/2026-05-31-compound-engineering-plugin-review.md). Before re-deriving a pattern, grep on the frontmatter (`applies_to`, `tags`, `problem_type`). Schema in [`.github/instructions/solutions.instructions.md`](.github/instructions/solutions.instructions.md).

## Don'ts

- Don't relitigate accepted ADRs.
- Don't write files outside their documented locations.
- Don't add features, refactor, or abstract beyond what the task requires.
- Don't write multi-paragraph commit messages or "covering your assets" hedge prose.
- Don't commit without being asked.
