---
id: META-005
status: backlog
subsystem: meta
tier: design
created: 2026-05-31
updated: 2026-05-31
spec: null
plan: null
blockers: []
---

## Context

**For a fresh agent with no prior conversation context:** this ticket exists because we reviewed the Every Inc compound-engineering plugin and identified its **Parallel Safety Check** protocol as worth borrowing. Background context lives at [`docs/solutions/external-reviews/2026-05-31-compound-engineering-plugin-review.md`](../solutions/external-reviews/2026-05-31-compound-engineering-plugin-review.md) — read that first.

**Problem this solves:** Our [README workflow](../../README.md) says "use git worktrees for parallel sessions" but doesn't engineer the mechanics. When two subagents work in parallel and both touch the same file, the failure modes are: shared-directory mode silently loses one agent's work (last writer wins); worktree-isolated mode produces a merge conflict that, if hand-resolved, silently picks a side and discards intent. The Every Inc plugin's `/ce-work` skill specifies a concrete protocol that prevents both failure modes by detecting overlap up-front and either downgrading to serial dispatch (no worktrees) or aborting and re-dispatching on conflict (with worktrees).

The protocol summary from `ce-work` SKILL.md:

1. Build a file-to-unit mapping from every candidate unit's `Files:` section.
2. Check for intersection — any path in 2+ units = overlap.
3. If overlap AND no worktree isolation: downgrade to serial subagents.
4. If overlap AND worktrees available: parallel dispatch is still safe; surface predicted conflicts and handle via abort + re-dispatch.
5. After parallel batch: merge subagent branches in dependency order; on conflict → `git merge --abort`, re-dispatch the conflicting unit serially against the now-merged tree. **Never hand-resolve** — it silently picks a side.

**Adaptation needed for our project:** our story files do not yet have a `## Files` section listing which files a plan will touch. The Parallel Safety Check needs that input. Adoption likely involves:

- A new required `## Files` section in the ticket story file schema, or an optional one when tier supports parallel dispatch.
- A new file-scoped instructions doc at `.github/instructions/parallel-dispatch.instructions.md` describing the safety-check decision tree and the merge-conflict mechanics.
- Optionally a helper script `tools/board/parallel_check.py` that takes 2+ ticket IDs and reports any file overlap.
- README workflow update: the existing "Parallelism model" subsection points at the new protocol.

Tier is `design` (not `small`) because this is a real protocol with multiple files, a behavioural contract for orchestrator agents, and design choices (does `## Files` belong in every story file, or only on tickets that may be parallelised? do we need the helper script?) deserve explicit thought.

**Reference URLs** (deep links for verification):

- Background review: [`docs/solutions/external-reviews/2026-05-31-compound-engineering-plugin-review.md`](../solutions/external-reviews/2026-05-31-compound-engineering-plugin-review.md)
- Primary external source (search for "Parallel Safety Check" and "Subagent dispatch"): <https://github.com/EveryInc/compound-engineering-plugin/blob/main/plugins/compound-engineering/skills/ce-work/SKILL.md>
- See also: subagent isolation modes (worktree vs shared-directory fallback) in the same file.

## Acceptance

- A spec exists at `docs/superpowers/specs/META-005-parallel-safety-check-design.md` covering:
  - How `## Files` is captured in story files (required, optional, or tier-conditional).
  - The safety-check decision tree (file mapping → intersection → worktree-or-not → dispatch mode).
  - The worktree-isolated vs shared-directory dispatch protocols.
  - The post-batch merge / abort / re-dispatch flow.
- A plan exists at `docs/superpowers/plans/META-005-parallel-safety-check-plan.md`.
- [`.github/instructions/tickets.instructions.md`](../../.github/instructions/tickets.instructions.md) gains the `## Files` requirement (per the design decision).
- A new [`.github/instructions/parallel-dispatch.instructions.md`](../../.github/instructions/) documents the orchestrator protocol.
- README "Parallelism model" subsection in the Workflow section references the new instructions file.
- At least one existing story file is updated with `## Files` as a worked example.

## Plan

(Tier `design` — this section is a brief summary; the full plan lives at the path linked from `plan:` front-matter once written.)

1. Read [`docs/solutions/external-reviews/2026-05-31-compound-engineering-plugin-review.md`](../solutions/external-reviews/2026-05-31-compound-engineering-plugin-review.md) and the `ce-work` SKILL.md sections linked above.
2. Run the brainstorming step (`superpowers:brainstorming` for Claude; manual chat for Copilot) → spec at `docs/superpowers/specs/META-005-parallel-safety-check-design.md`. Update `spec:` front-matter to point at it.
3. Run the plan-writing step → plan at `docs/superpowers/plans/META-005-parallel-safety-check-plan.md`. Update `plan:` front-matter.
4. Implement per the plan: add `## Files` to ticket schema, write the parallel-dispatch instructions, update README, retrofit a worked example.
5. Commit as: `docs: adopt Parallel Safety Check protocol for orchestrator agents (META-005)`.

## Log

- 2026-05-31: created. Status: backlog. Tier: design. Source: review of EveryInc/compound-engineering-plugin captured in `docs/solutions/external-reviews/`.
