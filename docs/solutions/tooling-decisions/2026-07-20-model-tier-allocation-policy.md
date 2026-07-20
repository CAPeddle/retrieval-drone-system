---
title: "Model-tier allocation: frontier model plans, cheaper tiers execute"
captured: 2026-07-20
applies_to: ["*"]
tags: [agent-workflow, model-tiers, planning, goal-execution, capability-allocation]
problem_type: reference
source: internal
applies_when: "Choosing which model tier runs a session, or writing a plan that a cheaper model will execute"
---

# Model-tier allocation: frontier model plans, cheaper tiers execute

## Summary

The highest-capability model available (Fable, capped at ~50% of subscription usage as of 2026-07-20) is reserved for work where the capability gap versus cheaper tiers is largest and the output is durable: deep planning, design, safety review, and subtle-failure diagnosis. Cheaper tiers execute. The hinge that makes the split work: plans are written **executor-agnostic** — every design fork pre-decided, so execution requires no architectural judgment.

## Context

Prompted by the frontier model's usage cap. Rather than triaging "final hours", the cap makes tier allocation a standing policy. The v0.3 vertical slice is the existence proof: a frontier-written plan (17 units, every fork pre-decided, review-hardened) was executed downstream across four phases and ~180 tests with zero design questions escalating back.

## The allocation

**Frontier tier (the capped budget):**

- Deep `ce-plan` runs on fork-dense or safety-critical clusters (next in queue: the laser-detector cluster, TRK-009a–d)
- ADR drafting and revision
- Adversarial safety reviews (the multi-persona verify passes that caught the `decomposeHomographyMat` misuse and the hysteresis self-contradiction at plan time)
- Replay-gate-failure diagnosis — the class where the failure is calibration signal and the cause may be a stale premise or geometry subtlety, not a code bug

**Cheaper tiers:**

- `/goal` execution of implementation-ready plans (Opus-tier precedent established)
- Mechanical edits, ticket/board bookkeeping, formatting
- Compound-run research subagents (Sonnet-tier precedent established)
- Recording and verification script runs

## Trade-offs

- The split is enforced only by the operator's session-start model choice; nothing mechanical prevents burning frontier budget on mechanical work. The mitigation is habit plus this doc.
- Executor-agnostic plans cost more at planning time (every fork must be closed). That cost is the point: it moves judgment to the tier that has it, once, instead of paying it per execution step.
- Frontier-tier review of cheap-tier output is not in the standing budget; the plan's verification contracts (gates, budgets, replay suites) stand in for it.

## Source links

- Existence proof: `docs/plans/2026-07-18-001-feat-v03-vertical-slice-plan.md` (planned on the frontier tier; executed downstream)
- Plan-time catches that justify frontier review: [camera centre from a world-plane homography](../cpp/2026-07-18-camera-centre-from-plane-homography.md), the KTD-8 hysteresis resolution in that plan's history
- Diagnosis-class example: [replay-gate premises are scene assertions](../workflow/2026-07-19-replay-gate-premises-are-scene-assertions.md)
