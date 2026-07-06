---
description: ADR conventions — anti-patterns, file naming, and design-doc sync. Applies when editing ADRs.
paths:
  - "docs/adr/*.md"
---

# ADR Conventions

How/when to *write* an ADR lives in the `adr-creation` skill; this rule covers
the durable conventions that apply whenever you touch `docs/adr/`.

## ADR anti-patterns — do not write these

- **Covering Your Assets:** vague decisions with escape clauses ("we might
  consider X if conditions warrant"). The Decision section must commit; hedging
  belongs in Risks, not Decision.
- **Groundhog Day:** failing to record Alternatives Considered, so the same
  debate recurs. Always document what was rejected and why.
- **Email-Driven:** load-bearing decisions captured only in conversation. If a
  decision qualifies for an ADR, the conversation is not enough — write the ADR.

**Never cite ADR-009 as settled.** ADR-009 (active calibration refinement) is
Proposed, not authoritative. Do not reference it in new ADRs, code comments, or
design docs; if active calibration becomes relevant, raise it with the user as an
open question, not a planned feature.

## File naming & numbering

- File name: `ADR-NNN-short-title-with-hyphens.md` (three-digit zero-padded,
  kebab-case title).
- Numbers are sequential, never reused, never renumbered. Once committed, a
  number is permanent even if the ADR is later superseded.
- ADRs are never deleted. Superseded ADRs stay with status
  `Superseded by ADR-MMM`.
- `docs/adr/README.md` is the index — add a row when creating a new ADR. The
  README is the only file in the directory edited as ADRs are added; ADR files
  themselves are append-only beyond status updates.

## Design-doc sync

The consolidated v0.3 design document restates ADR decisions in readable prose.
When an ADR changes status, update the design document in the same commit:
- New ADR accepted → add a reference in the appropriate section.
- ADR superseded → point references to the superseding ADR; do NOT delete the old
  reference, frame it as "previously ADR-NNN, now superseded by ADR-MMM."
- Design doc vs ADR conflict → the ADR wins; update the design doc. This is a
  documentation bug, not a design question.
