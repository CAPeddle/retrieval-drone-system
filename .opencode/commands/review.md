---
description: First-pass review of the current working-tree diff
agent: build
---

Diff under review:
!`git diff`

Dispatch `@reviewer` on the diff above. If it touches the `safe_for_control` predicate, ADR-007, coordinate mapping, detection, or the C++ hot path, also dispatch `@safety-reviewer`.

Consolidate the findings by severity (Blocker / Major / Minor), each with a `file:line` reference and a concrete fix. Cite ADRs by ID. End by stating whether this change should be escalated to a final Claude Code review and what specifically to re-check there. Do not edit anything — this is review only.
