---
id: META-004
status: backlog
subsystem: meta
tier: small
created: 2026-05-31
updated: 2026-05-31
spec: null
plan: null
blockers: []
---

## Context

**For a fresh agent with no prior conversation context:** this ticket exists because we recently reviewed the Every Inc compound-engineering plugin and identified its "Implementation Units with U-IDs" pattern as worth borrowing. Background context lives at [`docs/solutions/external-reviews/2026-05-31-compound-engineering-plugin-review.md`](../solutions/external-reviews/2026-05-31-compound-engineering-plugin-review.md) — read that first.

**Problem this solves:** Our story files (`docs/tickets/<ID>-<slug>.md`) have a `## Plan` section with numbered steps (`1.`, `2.`, ...). If a step is added, removed, or reordered, the numeric positions shift — but references to "step 3" in subagent dispatch prompts, `## Log` entries, or blocker descriptions then point at the wrong step. Compound-engineering solves this with stable **Unit IDs (U-IDs)** — each step gets a permanent identifier like `U1`, `U2`, `U3` that never gets reused even if a step is removed or reordered.

**Reference URLs** (deep links for verification):

- Background review: [`docs/solutions/external-reviews/2026-05-31-compound-engineering-plugin-review.md`](../solutions/external-reviews/2026-05-31-compound-engineering-plugin-review.md)
- Primary external source (search for "U-ID" and "Implementation Units"): <https://github.com/EveryInc/compound-engineering-plugin/blob/main/plugins/compound-engineering/skills/ce-work/SKILL.md>
- Plan-side U-ID convention origin: <https://github.com/EveryInc/compound-engineering-plugin/blob/main/plugins/compound-engineering/skills/ce-plan/SKILL.md>

## Acceptance

- [`.github/instructions/tickets.instructions.md`](../../.github/instructions/tickets.instructions.md) `## Required Sections → ## Plan` subsection documents the U-ID convention. The example in that file shows `U1. Do thing` instead of `1. Do thing`.
- U-IDs are monotonic integers, prefixed with `U`. Never reused. If `U2` is removed, the next new step is `U7` (or whatever the next-monotonic value is), not a recycle of `U2`.
- A short note in [`.github/instructions/board.instructions.md`](../../.github/instructions/board.instructions.md) or the [README workflow section](../../README.md) explains that `## Log` entries and blocker descriptions reference U-IDs, not step numbers.
- At least one existing story file under [`docs/tickets/`](../) is retrofitted with U-IDs as a worked example. Recommend [TRK-001](TRK-001-rename-tracking-system-to-tracking-core.md) since it has the longest plan among current Next-column tickets.
- `tools/board/board_check.py` (when it exists) does not reject pre-U-ID plans on existing tickets — backward compatibility for tickets created before this change. New tickets created after this change must use U-IDs.

## Plan

(Tier `small`, plan inline.)

1. Read the background review file linked in Context.
2. Fetch the two SKILL.md files linked above to understand exactly how U-IDs are structured and used in compound-engineering.
3. Edit `.github/instructions/tickets.instructions.md`: in the `## Plan` subsection of `## Required Sections`, add the U-ID convention with a concrete example.
4. Edit `.github/instructions/board.instructions.md` OR the README workflow section to note that `## Log` entries cite U-IDs (e.g., `- 2026-06-01: U2 complete. Tests pass.`).
5. Retrofit TRK-001 — rewrite its `## Plan` with U-IDs preserved in original numeric order.
6. Commit as: `docs: adopt U-IDs in ticket story plans (META-004)`.

No code changes. No script changes. Pure documentation + convention.

## Log

- 2026-05-31: created. Status: backlog. Source: review of EveryInc/compound-engineering-plugin captured in `docs/solutions/external-reviews/`.
