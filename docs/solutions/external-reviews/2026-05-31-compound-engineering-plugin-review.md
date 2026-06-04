---
title: Compound-Engineering Plugin — Critical Review
captured: 2026-05-31
applies_to: ["*"]
tags: [agent-workflow, plan-driven-development, parallel-subagents, kanban, learnings, multi-agent-tooling]
problem_type: review
source: https://github.com/EveryInc/compound-engineering-plugin
---

## Summary

The Every Inc compound-engineering plugin is a mature, production-grade workflow toolkit for AI-assisted development. We reviewed it as a potential alternative to (or improvement over) our own kanban + story-file workflow. Net conclusion: **don't adopt wholesale, cherry-pick high-value patterns.** Their tooling is sized for a different context — single product, JS/Bun ecosystem, Claude/Codex/Cursor target, no formal ADR culture. Our workflow has one specific thing they don't model: multi-subsystem boundaries with formal ADR governance, and that is deliberately load-bearing here.

Reach for this entry when: evaluating another external workflow plugin, deciding whether to import their patterns, or onboarding a new agent who wants to know why our process looks different from compound-engineering's.

## Context

We had built a workflow with: kanban [BOARD.md](../../../BOARD.md), per-ticket story files in [`docs/tickets/`](../../tickets/), file-scoped instructions in [`.github/instructions/`](../../../.github/instructions/) enforcing schema for both Claude Code and GitHub Copilot, and a planned set of Python helper scripts. User asked whether the compound-engineering plugin was an alternative we should adopt instead. This review captures the comparison and what we chose to borrow.

## What compound-engineering is

A Bun/TypeScript plugin published by Every (every.to). Ships **37 skills + 51 specialised review agents** in the `compound-engineering` plugin. Headline loop: `strategy → ideate → brainstorm → plan → work → code-review → compound`. The headline philosophy: "each unit of engineering work should make subsequent units easier — not harder."

Also ships a CLI that converts Claude-native skills to Codex, Cursor, OpenCode formats. **GitHub Copilot is NOT in their conversion target list** — adopting their skills for Copilot would still require hand-porting to `.github/instructions/`.

## Where they are meaningfully ahead

| Their thing | Why it matters |
|---|---|
| **Implementation Units with U-IDs in plans** | Stable identifiers (`U1`, `U2`, ...) survive plan edits. Solves the ticket/step renumbering problem when plans evolve. |
| **Parallel Safety Check protocol** | File-to-unit mapping → intersection detection → worktree-isolated vs shared-directory dispatch with explicit conflict-resolution rules. Concrete engineering of merge-safe parallelism. |
| **Subagent dispatch templates** | Explicit spec for what to pass each subagent, commit ownership in worktree vs shared modes, abort+redispatch on conflict (never silent merge). |
| **Test scenario completeness check** | Happy/edge/error/integration categorisation — check plan covers them, supplement gaps before writing tests. |
| **`STRATEGY.md` upstream anchor** | One-page product anchor read by ideate/brainstorm/plan as grounding context. (Our CLAUDE.md is too big to play this role.) |
| **`docs/solutions/` learnings database** | YAML-frontmatter recipes with `module`/`tags`/`problem_type`. Queryable patterns library. Complements ADRs. **We adopted this — you're reading the first entry.** |
| **Trivial routing** | Explicit "skip the full workflow on a typo fix." Maps onto our `tier: mechanical`. |

## Where we have things they don't

- **Persistent visible kanban.** Their "board" is the platform task tracker — session-scoped, invisible between sessions. Our [BOARD.md](../../../BOARD.md) is durable and human-scannable.
- **Story files with append-only Log.** They explicitly forbid editing plan body during execution. We need the Log for cross-session visibility.
- **Multi-subsystem boundary model.** Their workflow assumes one product. We have 6+ subsystems with separate lifecycles, ADRs spanning them, contract-bound seams.
- **Formal ADR governance.** They use solutions/compounds but have no ADR concept. For a project whose CLAUDE.md explicitly says "do not relitigate accepted ADRs," ADRs are load-bearing.

## Conflicts with our constraints

1. **Bun/TypeScript dependency.** Their CLI requires `bun install`. We're a C++/Python embedded project. Adding a Node toolchain just for plugin management is dead weight.
2. **No Copilot target.** Adopting wholesale would mean hand-porting half the surface to file-scoped instructions.
3. **Skill volume.** 37 + 51 = 88 artifacts; most are domain-targeted (Rails, iOS, Swift, Figma, Slack). Plausibly useful for our project: maybe 10%.
4. **Default ceremony.** Full loop assumes substantial features. Acknowledged by their Trivial routing but the baseline weight is real.

## What we adopted (with ticket IDs)

- **`docs/solutions/` learnings database** — adopted in this session (this very file is the first entry). Pattern documented in [`.github/instructions/solutions.instructions.md`](../../../.github/instructions/solutions.instructions.md).
- **U-IDs in story plans** — backlogged as [META-004](../../tickets/META-004-adopt-u-ids-in-plans.md).
- **Parallel Safety Check protocol** — backlogged as [META-005](../../tickets/META-005-adopt-parallel-safety-check.md) (tier=design — needs spec work).

## What we explicitly did NOT adopt

- The bun/TypeScript CLI and the plugin install pipeline.
- The full 37-skill + 51-agent inventory. Cherry-pick if/when a specific need arises.
- `STRATEGY.md` as a replacement for [CLAUDE.md](../../../CLAUDE.md). They serve different roles; CLAUDE.md is operating contract, not product anchor. May add a separate STRATEGY.md later if useful.
- Their no-edit-plan-during-execution rule. Our story files' `## Log` section is load-bearing for multi-session visibility.
- `/ce-work` as a wholesale execution skill. Our kanban + story file model is a different mental model; reconciling the two costs more than it gains.

## Source links (deep, for verification by future agents)

- Repo root: <https://github.com/EveryInc/compound-engineering-plugin>
- README (workflow loop overview): <https://github.com/EveryInc/compound-engineering-plugin/blob/main/README.md>
- AGENTS.md (repo conventions, contains the `docs/solutions/` category guidance we borrowed): <https://github.com/EveryInc/compound-engineering-plugin/blob/main/AGENTS.md>
- `ce-work` SKILL.md (search for "Parallel Safety Check", "U-ID", "Implementation Units"): <https://github.com/EveryInc/compound-engineering-plugin/blob/main/plugins/compound-engineering/skills/ce-work/SKILL.md>
- `ce-plan` SKILL.md (defines the plan structure incl. U-IDs): <https://github.com/EveryInc/compound-engineering-plugin/blob/main/plugins/compound-engineering/skills/ce-plan/SKILL.md>
- Plugin component reference: <https://github.com/EveryInc/compound-engineering-plugin/blob/main/plugins/compound-engineering/README.md>

## When this review might become stale

- They add Copilot as a conversion target — re-evaluate the wholesale-adoption question.
- They change their kanban posture (currently no persistent board) — may unify with our model.
- We grow beyond solo dev and start needing more of their adversarial-reviewer agents — re-evaluate which agents to import as Copilot/Claude prompt templates.

Capture next review with new date prefix in this directory; do not edit this entry beyond clerical corrections.
