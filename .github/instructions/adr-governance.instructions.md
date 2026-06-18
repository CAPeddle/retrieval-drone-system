---
description: "Use when drafting ADRs, proposing architectural changes, editing design or governance documents, or when a request may contradict an existing ADR."
---
# ADR Governance

## Source of Truth Hierarchy

1. Current conversation with the user.
2. Accepted ADRs in `docs/adr/`.
3. Consolidated design document (when it exists).
4. Code and committed configuration.
5. Proposed ADRs and historical docs (rank 5 reference material).

When a lower rank contradicts a higher, the higher wins. When the user contradicts an ADR, update the ADR/docs **before** implementing.

## When to Write an ADR

Write one when the decision is architecturally significant — changing it later would be expensive. Triggers:

- New external system contract (MCU, transport, ZMQ consumer).
- Change to an existing locked decision (new ADR supersedes old).
- New component boundary with its own lifecycle/threading/failure semantics.
- Non-functional property commitment (latency budget, accuracy target).
- Non-obvious trade-off where rejected alternatives must be remembered.

Do NOT write ADRs for: variable naming, internal helper structure, library choice within an established category, bug fixes without contract changes.

## Template (Nygard / Richards-Ford)

```markdown
# ADR-NNN: Short Title

## Status
[Proposed | Accepted | Superseded by ADR-MMM] (date)

## Context
Forces at play — technical, hardware, environmental, contractual.

## Decision
Active voice. Specific. Falsifiable. Include parameters and thresholds.

## Consequences
**Positive:** What becomes easier.
**Negative:** What becomes harder.
**Risks:** Specific failure modes with mitigations.

## Alternatives Considered
What was rejected and why. This prevents Groundhog Day.

## Related ADRs
Cross-references.
```

## Anti-Patterns

- **Covering Your Assets:** vague decisions with escape clauses. The Decision section must commit.
- **Groundhog Day:** no Alternatives Considered → same debate recurs.
- **Email-Driven:** decision made in conversation but never recorded as an ADR.

## File Naming

`ADR-NNN-short-title-with-hyphens.md`. Three-digit zero-padded, kebab-case. Numbers sequential, never reused, never renumbered. Superseded ADRs keep their file with status updated.

## Changing Locked Decisions

Never edit an accepted ADR in place (beyond clerical). Write a new ADR that supersedes it and update the old ADR's status to `Superseded by ADR-NNN`.
