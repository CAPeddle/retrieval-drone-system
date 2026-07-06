---
name: adr-creation
description: Use when a decision is architecturally significant — a new external contract, a change to a locked decision, a new module boundary, a non-functional commitment, or a non-obvious trade-off worth remembering. Guides when to write an ADR and drafts it against the project template.
---

# ADR Creation

Write an ADR when a decision is architecturally significant — when changing it
later would be expensive or disruptive.

## When to write one (§8.1)

Concrete triggers:
- A new external system contract (a new MCU, transport, or ZMQ-stream consumer).
- A change to an existing locked decision (the new ADR supersedes the old one —
  update the old ADR's status to `Superseded by ADR-NNN`).
- A new component boundary with its own lifecycle, threading model, or failure
  semantics.
- A non-functional commitment (latency budget, accuracy target, safety predicate).
- A non-obvious trade-off whose rejected alternatives must be remembered to
  prevent relitigation.

Do NOT write one for: variable/function naming; internal helper structure; library
choice within an established open category (use a code comment or benchmark note);
bug fixes, unless they change a contract.

When the trigger is clearly met, draft the ADR without asking. When it's genuinely
unclear whether a decision is load-bearing, ask the user a short question rather
than either spamming ADRs or hiding a load-bearing decision in a comment.

## Template (§8.2) — Nygard / Richards-Ford

The template is not optional. An ADR missing a Status line, Alternatives
Considered, or explicit Consequences is incomplete.

```
# ADR-NNN: Short Title

## Status
[Proposed | Accepted | Superseded by ADR-MMM] (date)

## Context
The issue motivating this decision and the forces at play — technical, hardware,
environmental, contractual. Cite related ADRs.

## Decision
Active voice. "We will use X." Specific — include parameters, thresholds, named
conditions. Falsifiable: a reader should be able to look at the running system
and tell whether the decision is in effect.

## Consequences
**Positive:** what becomes easier.
**Negative:** what becomes harder.
**Risks:** specific failure modes introduced, with mitigations where known.

## Alternatives Considered
What else was on the table and why it was rejected. This section prevents
Groundhog Day.

## Related ADRs
Cross-references to ADRs this one depends on or affects.
```

## After drafting

- Use the next free number (`docs/adr/`). Numbers are sequential, never reused or
  renumbered.
- Add a row to `docs/adr/README.md` (the index).
- If the decision changes a locked ADR, update that ADR's status and update the
  v0.3 design document's references in the same commit.
- File naming and the anti-patterns to avoid are in the `project-docs` rule,
  which auto-applies when you edit files under `docs/adr/`.
