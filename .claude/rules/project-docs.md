---
description: ADR conventions (anti-patterns, file naming, design-doc sync) and ticket/board tooling. Applies when editing ADRs, tickets, or BOARD.md.
paths:
  - "docs/adr/*.md"
  - "docs/tickets/*.md"
  - "BOARD.md"
---

# Project-Docs Rules

Conventions for ADRs and the ticket board. How/when to *write* an ADR lives in
the `adr-creation` skill; this rule covers the durable conventions that apply
whenever you touch these files.

## ADR anti-patterns — do not write these (§8.3)

- **Covering Your Assets:** vague decisions with escape clauses ("we might
  consider X if conditions warrant"). The Decision section must commit; hedging
  belongs in Risks, not Decision.
- **Groundhog Day:** failing to record Alternatives Considered, so the same
  debate recurs. Always document what was rejected and why.
- **Email-Driven:** load-bearing decisions captured only in conversation. If a
  decision qualifies for an ADR, the conversation is not enough — write the ADR.

## ADR file naming & numbering (§8.4)

- File name: `ADR-NNN-short-title-with-hyphens.md` (three-digit zero-padded,
  kebab-case title).
- Numbers are sequential, never reused, never renumbered. Once committed, a
  number is permanent even if the ADR is later superseded.
- ADRs are never deleted. Superseded ADRs stay with status
  `Superseded by ADR-MMM`.
- `docs/adr/README.md` is the index — add a row when creating a new ADR. The
  README is the only file in the directory edited as ADRs are added; ADR files
  themselves are append-only beyond status updates.

## Design-doc sync (§8.5)

The consolidated v0.3 design document restates ADR decisions in readable prose.
When an ADR changes status, update the design document in the same commit:
- New ADR accepted → add a reference in the appropriate section.
- ADR superseded → point references to the superseding ADR; do NOT delete the old
  reference, frame it as "previously ADR-NNN, now superseded by ADR-MMM."
- Design doc vs ADR conflict → the ADR wins; update the design doc. This is a
  documentation bug, not a design question.

## Ticket / board tooling (§11.7)

`tools/board/` holds stdlib-only Python helpers that automate the kanban
operations defined in `.github/instructions/board.instructions.md` and
`tickets.instructions.md`. Prefer the scripts over hand-editing BOARD.md or story
files — they perform the atomic update of `status:`, `updated:`, the `## Log`
entry, and the BOARD line in one operation.

- `python tools/board/ticket_new.py PREFIX-NNN "Title" [--tier mechanical|small|design]`
- `python tools/board/ticket_move.py PREFIX-NNN <status> [--note "..."]`
- `python tools/board/board_check.py` — lints BOARD.md against story files; exits 1 on mismatch.
- `python tools/board/ticket_archive.py [--keep N]`

Full usage in `tools/board/README.md`. Hand-edits remain permitted for anything
the scripts cannot express, but the scripts are the default.
