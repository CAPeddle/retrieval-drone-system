# Solutions Library

Documented patterns, recipes, reviews, and recovery procedures captured from work on this project (or evaluated from external sources). Each solution is queryable by frontmatter — `applies_to`, `tags`, `problem_type` — so a future agent can find prior work without re-deriving it.

## Relationship to other doc collections

| Doc collection | Role |
|---|---|
| [`docs/adr/`](../adr/) | Architectural **decisions** — WHY we chose what we chose |
| `docs/solutions/` (this dir) | **Patterns, recipes, reviews, learnings** — HOW to do things, or what we observed |
| [`docs/research/`](../research/) | Historical v0.2 brainstorms — rank-5 reference per [CLAUDE.md §6](../../CLAUDE.md) |

## Categories

- [`workflow/`](workflow/) — Agent coordination, kanban tweaks, planning patterns
- [`cpp/`](cpp/) — Tracking-core C++ patterns and gotchas
- [`python/`](python/) — Viewer and tooling patterns
- [`calibration/`](calibration/) — Calibration drift recovery and procedures
- [`external-reviews/`](external-reviews/) — Critical reviews of external workflows or tools considered for borrowing

## File format

See [`.github/instructions/solutions.instructions.md`](../../.github/instructions/solutions.instructions.md) for the required frontmatter schema and conventions. Both Claude and Copilot enforce this via file-scoped instructions.

## Index (most recent first)

| Date | File | Type | Applies to | Tags |
|---|---|---|---|---|
| 2026-06-14 | [workflow/multi-agent-code-review-workflow.md](workflow/2026-06-14-multi-agent-code-review-workflow.md) | workflow_issue | * | code-review, multi-agent, opencode, workflow-improvement, agent-configuration |
| 2026-06-04 | [workflow/session-decision-log-2026-05-31.md](workflow/session-decision-log-2026-05-31.md) | reference | * | agent-workflow, decision-log, questions, session-history |
| 2026-06-04 | [workflow/tracking-core-rename-and-build-overhaul-handoff.md](workflow/2026-06-04-tracking-core-rename-and-build-overhaul-handoff.md) | recipe | tracking-core | workflow, build-system, handoff, ticket-state, validation |
| 2026-05-31 | [external-reviews/compound-engineering-plugin-review.md](external-reviews/2026-05-31-compound-engineering-plugin-review.md) | review | * | agent-workflow, plan-driven-development, parallel-subagents, kanban |
